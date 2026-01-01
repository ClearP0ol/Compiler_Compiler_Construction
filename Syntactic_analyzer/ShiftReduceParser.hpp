#ifndef SHIFTREDUCEPARSER_HPP
#define SHIFTREDUCEPARSER_HPP

#include "GrammarLoader.hpp"
#include "SLRAnalysisTable.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

/*
SEM_IR 宏：用于条件编译语义分析 + 中间代码(IR)生成相关逻辑。
- 关闭 SEM_IR：该文件只做纯 SLR 语法分析（状态栈+符号栈），不做语义检查/IR 输出。
- 开启 SEM_IR：在 SLR 解析流程的 SHIFT/REDUCE 动作中同步维护“语义值栈(ValueStack)”
	并维护符号表(Scopes)、IR 四元式(IR) 等辅助结构。
*/
#define SEM_IR

#ifdef SEM_IR
#include <variant>
#include <optional>
#include <set>
#endif

using namespace std;

// 移进-归约分析器
struct ShiftReduceParser
{

#ifdef SEM_IR
	// ====== 最小化语义类型系统 ======

	// BaseType：语义分析阶段使用的“静态类型”枚举，用于类型检查与符号表记录。
	// - INT/VOID/BOOL：示例语言最常见的几种类型
	// - ERR：语义推断失败/错误传播时的占位类型
	enum class BaseType { INT, VOID, BOOL, ERR };
	// 将 BaseType 转成可读字符串，便于报错与调试输出。
	static inline const char* TypeName(BaseType t) {
		switch (t) {
			case BaseType::INT:return "int";
			case BaseType::VOID:return "void";
			case BaseType::BOOL:return "bool";
			default:return "err";
		}
	}

	// ====== 三地址/四元式 IR ======
	/*
	Quad：中间代码的一个“四元式”单元，常用于三地址码表示。
	- op：操作符（例如 "+", "*", "=", "goto", "if<", "ret" 等）
	- a1/a2：操作数（可能是变量唯一名、临时变量、常量字面量等）
	- res：结果位置（目的变量/临时变量名；对 if/goto 可不使用）
	- target：跳转目标地址（用于 goto / ifxxx），-1 表示“占位”，需要后续 backpatch 填充
	*/
	struct Quad {
		string op, a1, a2, res;
		int target = -1; // goto/if 的目标，-1 表示占位
	};
	// IR：顺序保存整个程序产生的四元式列表；NextQuad() 即“下一条指令地址”。
	vector<Quad> IR;

	// nextquad：返回当前 IR 长度，即下一条将要 emit 的指令下标。
	int NextQuad()const { return (int)IR.size(); }
	/*
	Emit：追加一条四元式到 IR，并返回其下标。
	- 语义动作（reduce）中会频繁调用：表达式生成、赋值、跳转占位等。
	- target 默认为 -1，表示该四元式可能需要回填跳转目标。
	*/
	int Emit(const string& op, const string& a1 = "", const string& a2 = "", const string& res = "", int target = -1) {
		IR.push_back({ op,a1,a2,res,target });
		return (int)IR.size() - 1;
	}
	/*
	Merge：回填(list)合并工具。
	在经典 backpatching 中 truelist/falselist/nextlist 都是“未决跳转”的指令下标集合，
	需要在归约时合并列表。
	*/
	static inline vector<int> Merge(const vector<int>& a, const vector<int>& b) {
		vector<int> r = a; r.insert(r.end(), b.begin(), b.end()); return r;
	}
	/*
	Backpatch：回填函数，将 lst 中记录的四元式跳转目标统一填成 target。
	使用场景：
	- if 条件为真 -> 跳到 then 的入口
	- if 条件为假 -> 跳到 else 的入口或 if 后的位置
	- while 条件为真 -> 跳到循环体；为假 -> 跳出循环
	*/
	void Backpatch(const vector<int>& lst, int target) {
		for (int i : lst) {
			if (i < 0 || i >= (int)IR.size())continue;
			IR[i].target = target;
		}
	}

	// ====== 语义值（按需最小集）======
	/*
	语义值(semantic value)：与 SymbolStack 中每个语法符号“一一对齐”的属性记录。
	在 SLR/LR 中，语义动作通常在 REDUCE 时执行：
	- 从 ValueStack 弹出 RHS 的语义值数组 rhs[]
	- 计算 LHS 的语义值 lhsVal
	- 将 lhsVal 再压回 ValueStack，保持与符号栈同步
	*/

	// TypeVal：用于非终结符 Type 的综合属性（例如 "int" / "void"）
	struct TypeVal { BaseType t = BaseType::ERR; };
	// IdVal：用于终结符 id 的语义值
	// - name：保留原始 lexeme（例如 "x"），不能用 LookupSymbol.Name（那会被改成 "id"）
	// - pos：位置信息（用于报错定位）
	struct IdVal { string name, pos; };
	// NumVal：用于终结符 num 的语义值（保存整型常量值）
	struct NumVal { int v = 0; };
	/*
	ExprVal：表达式综合属性
	- t：表达式静态类型（用于算术/赋值/return 等类型检查）
	- place：表达式结果“放在哪里”
	  可能是：
		1) 变量的唯一名（Symbol.irName）
		2) 临时变量名（t1,t2,...）
		3) 常量字面量字符串（例如 "123"）
	- begin：该表达式对应 IR 的“起始指令地址”，用于控制流拼接时知道入口位置
	*/
	struct ExprVal { BaseType t = BaseType::ERR; string place; int begin = -1; };
	/*
	BoolVal：布尔表达式（这里用“控制流表示法”而非直接计算 true/false）
	- truelist：条件为真时跳转指令的占位集合
	- falselist：条件为假时跳转指令的占位集合
	- begin：该条件判断 IR 的入口位置
	*/
	struct BoolVal { vector<int> truelist, falselist; int begin = -1; };
	/*
	StmtVal：语句综合属性
	- nextlist：语句执行完后需要跳转但尚未确定目标的 goto 占位集合
	  （顺序语句拼接/控制流结构收尾时常用）
	- begin：该语句对应 IR 的入口位置
	*/
	struct StmtVal { vector<int> nextlist; int begin = -1; };
	// OpVal：关系运算符 RelOp 的语义值（保存 "<", ">=", "==" 等字符串）
	struct OpVal { string op; };
	/*
	SemVal：统一的“语义值载体”，使用 std::variant 让不同符号承载不同字段。
	- monostate：占位类型，表示该符号暂时不携带语义属性（例如 ";"、"("、")" 等）
	*/
	using SemVal = variant<monostate, TypeVal, IdVal, NumVal, ExprVal, BoolVal, StmtVal, OpVal>;
	/*
	ValueStack：语义值栈，与 SymbolStack 同步（对齐规则非常重要！）
	- SHIFT：压入终结符对应的语义值（id/num/type/op…），以及符号栈压入的 LookupSymbol
	- REDUCE：弹出 RHS 个语义值，计算 LHS 语义值再压回
	*/
	stack<SemVal> ValueStack;

	// 方便取 variant：As<T>(v) 获取，Is<T>(v) 判断类型
	template<class T>static inline const T& As(const SemVal& v) { return get<T>(v); }
	template<class T>static inline bool Is(const SemVal& v) { return holds_alternative<T>(v); }

	// ====== 符号表（作用域栈）======

	/*
	SymKind：符号种类
	- VAR：变量
	- FUNC：函数（这里仅记录返回类型/名字；参数类型可扩展）
	- PARAM：参数（属于函数体的局部符号）
	*/
	enum class SymKind { VAR, FUNC, PARAM };
	/*
	Symbol：符号表条目
	- kind：符号种类
	- type：变量/参数类型；函数返回类型
	- params：函数参数类型（本最小实现里仅做占位，参数先存在 PendingParams）
	- irName：变量唯一名（解决遮蔽/同名问题；IR 中使用 irName 不会混淆）
	- scopeLevel：所在作用域层级（调试/报错用）
	*/
	struct Symbol {
		SymKind kind = SymKind::VAR;
		BaseType type = BaseType::ERR;
		vector<BaseType> params;
		string irName;
		int scopeLevel = 0;
	};
	/*
	Scopes：作用域栈（每层一个 hash 表：name -> Symbol）
	- BeginScope：遇到 '{' 进入新块作用域
	- EndScope：遇到 '}' 退出作用域
	说明：这里只保留一个全局 scope（Scopes.size()>1 才 pop），避免空栈。
	*/
	vector<unordered_map<string, Symbol>> Scopes;
	// uniqId：用于生成临时变量名/变量唯一名，保证不冲突
	int uniqId = 0;

	void BeginScope() { Scopes.push_back({}); }
	void EndScope() {
		if (Scopes.size() > 1)Scopes.pop_back(); // 留一个全局 scope
	}

	/*
	Lookup：从内到外查找标识符，返回最近作用域中的符号条目。
	用途：检测“使用未定义标识符”、获取类型、获取 irName 用于生成 IR。
	*/
	Symbol* Lookup(const string& name) {
		for (int i = (int)Scopes.size() - 1; i >= 0; --i) {
			auto it = Scopes[i].find(name);
			if (it != Scopes[i].end())return &it->second;
		}
		return nullptr;
	}

	/*
	InsertHere：仅在当前作用域插入符号。
	用途：检测“同一作用域重定义”；允许外层同名在内层遮蔽（shadowing）。
	*/
	bool InsertHere(const string& name, const Symbol& sym, string& err) {
		auto& cur = Scopes.back();
		if (cur.find(name) != cur.end()) {
			err = "重定义标识符: " + name;
			return false;
		}
		cur[name] = sym;
		return true;
	}

	// NewTemp：生成一个新的临时变量名，用于表达式计算结果保存
	string NewTemp() { return "t" + to_string(++uniqId); }
	// NewVarName：为变量/参数生成一个唯一名（包含 scopeLevel 和序号）
	// 这样 IR 中使用 irName，可以避免块作用域同名变量冲突。
	string NewVarName(const string& raw) { return raw + "@" + to_string((int)Scopes.size() - 1) + "#" + to_string(++uniqId); }

	// ====== 函数上下文（最小化）======
	/*
	由于语法动作在 SHIFT/REDUCE 的某些时刻触发，而函数定义涉及“先看到返回类型+函数名+参数列表，
	再遇到 '{' 才进入函数体作用域”，因此需要一组“跨步骤的临时状态”保存上下文。
	*/

	bool PendingFunc = false; // 已识别到函数头（Type id '('），但还没遇到 '{' 建函数体作用域
	bool InFunction = false; // 当前是否在函数体内部（用于 return 合法性检查）
	string CurFuncName; // 当前函数名（可用于报错/生成标签）
	BaseType CurFuncRet = BaseType::ERR; // 当前函数返回类型
	int FuncScopeDepth = 0; // 用于判断函数体结束：作用域弹出到小于该值时即函数结束
	vector<pair<string, BaseType>> PendingParams; // 参数在参数列表归约时收集，等进入 '{' 后再插入到作用域
	/*
	PendingIfElseEndJumps：处理 if-else 的一个“最小化 hack”：
	- 在 SHIFT 到 else 之前，先 emit 一个 "goto _" 用来跳过 else（then 分支结束跳到 if-else 末尾）
	- 这个 goto 的下标需要跨越若干次 shift/reduce 保存，直到 else 语句归约完成后才能 backpatch 到 end
	*/
	stack<int> PendingIfElseEndJumps;

	// 打印 IR
	void DumpIR() const {
		cout << "\n==== IR quads ====\n";
		for (int i = 0; i < (int)IR.size(); ++i) {
			auto& q = IR[i];
			cout << i << ": (" << q.op;
			if (q.op == "goto") {
				cout << ", _, _, " << q.target << ")";
			}
			else if (q.op.rfind("if", 0) == 0) {
				cout << ", " << q.a1 << ", " << q.a2 << ", " << q.target << ")";
			}
			else if (q.op == "=") {
				cout << ", " << q.a1 << ", _, " << q.res << ")";
			}
			else {
				cout << ", " << q.a1 << ", " << q.a2 << ", " << q.res << ")";
			}
			cout << "\n";
		}
	}

	// 打印符号表：用于检查作用域/重定义/唯一名生成是否符合预期
	void DumpSymbols() const {
		cout << "\n==== Symbol Tables ====\n";
		for (int i = 0; i < (int)Scopes.size(); ++i) {
			cout << "-- scope " << i << " --\n";
			for (auto& [k, v] : Scopes[i]) {
				cout << k << " kind=" << (v.kind == SymKind::FUNC ? "func" : (v.kind == SymKind::PARAM ? "param" : "var"))
					<< " type=" << TypeName(v.type) << " ir=" << v.irName << "\n";
			}
		}
	}
#endif

	const SLRAnalysisTableBuilder& TableBuilder;
	const GrammarDefinition& Grammar;

	stack<int> StateStack;			  // 状态栈
	stack<GrammarSymbol> SymbolStack; // 符号栈

	// 辅助函数：根据产生式ID获取产生式
	const Production& GetProductionById(int prodId) const
	{
		for (const auto& prod : Grammar.Productions)
		{
			if (prod.Id == prodId)
			{
				return prod;
			}
		}
		throw runtime_error("未找到ID为" + to_string(prodId) + "的产生式");
	}

	// 辅助函数：打印当前栈状态
	void PrintStacks() const
	{
		// 打印状态栈
		cout << "状态栈: [ ";
		stack<int> TempStateStack = StateStack;
		vector<int> States;
		while (!TempStateStack.empty())
		{
			States.push_back(TempStateStack.top());
			TempStateStack.pop();
		}
		for (auto It = States.rbegin(); It != States.rend(); ++It)
		{
			cout << *It << " ";
		}
		cout << "]\n";

		// 打印符号栈
		cout << "符号栈: [ ";
		stack<GrammarSymbol> TempSymbolStack = SymbolStack;
		vector<GrammarSymbol> Symbols;
		while (!TempSymbolStack.empty())
		{
			Symbols.push_back(TempSymbolStack.top());
			TempSymbolStack.pop();
		}
		for (auto It = Symbols.rbegin(); It != Symbols.rend(); ++It)
		{
			cout << It->Name;
			if (!It->TokenType.empty())
			{
				cout << "(" << It->TokenType << ")";
			}
			cout << " ";
		}
		cout << "]\n";
	}

	// 构造函数
	ShiftReduceParser(const SLRAnalysisTableBuilder& tableBuilder)
		: TableBuilder(tableBuilder), Grammar(tableBuilder.Grammar)
	{
		// 初始化状态栈和符号栈
		StateStack.push(0);							// 初始状态为0
		SymbolStack.push(GrammarSymbol("$", true)); // 栈底符号为$

#ifdef SEM_IR
		// 语义值栈必须与符号栈保持严格对齐：
		// - 符号栈底已经压了 "$"
		// - 因此语义值栈也压一个 monostate 占位，与 "$" 对齐
		ValueStack.push(monostate{});

		// 初始化全局作用域（Scopes[0]），后续遇到 '{' 再 BeginScope() 进入块作用域
		Scopes.clear();
		Scopes.push_back({});
#endif
	}

	// 解析函数：接收符号序列，返回是否解析成功
	bool Parse(const vector<GrammarSymbol>& inputSymbols)
	{
		cout << "开始移进-归约分析。\n";

		size_t InputIndex = 0;
		const GrammarSymbol& EndSymbol = TableBuilder.FFCalculator.EndSymbol;

		while (true)
		{
			// 获取当前状态和当前输入符号
			int CurrentState = StateStack.top();
			const GrammarSymbol& CurrentInput = (InputIndex < inputSymbols.size()) ? inputSymbols[InputIndex] : EndSymbol;

			// 处理ID和NUM
			GrammarSymbol LookupSymbol = CurrentInput;
			if (CurrentInput.TokenType == "ID") {
				LookupSymbol.Name = "id";
			}
			else if (CurrentInput.TokenType == "NUM") {
				LookupSymbol.Name = "num";
			}

			cout << "\n当前状态: " << CurrentState << ", 当前输入符号: " << CurrentInput.Name << "\n";
			PrintStacks();

			// 查找ACTION表
			const SLRAction& Action = TableBuilder.GetAction(CurrentState, LookupSymbol);

			if (Action.Type == SLRActionType::SHIFT)
			{
				cout << "执行移进操作: S" << Action.StateOrProduction << "\n";
				
#ifdef SEM_IR
				/*
				(0) shift 前置动作：专门处理 "else"
				原因：
				- if-else 的控制流回填需要一个 “then 结束后跳过 else” 的 goto 占位；
				- 同时还要把条件的 falselist 回填到 else 的入口；
				- 但在纯 LR reduce 触发点，有时很难准确拿到“else 的入口地址”，因此这里采用最小化做法：
				  在真正 SHIFT else 之前（else 尚未入栈、else 的 IR 尚未产生），先：
				  1) emit goto _：保存到 PendingIfElseEndJumps（then 分支结束跳到 end）
				  2) backpatch 条件真到 then.begin
				  3) backpatch 条件假到 else.begin（此刻 NextQuad() 即 else 第一句的地址）
				*/
				if (CurrentInput.Name == "else") {
					// 复制符号栈/语义栈到数组，用模式匹配定位最近的 if (RelExpr) Stmt
					// 这是“最小化 hack”，不依赖产生式 id，但对栈形态有假设
					auto CopySyms = SymbolStack; vector<GrammarSymbol> syms;
					auto CopyVals = ValueStack; vector<SemVal> vals;
					while (!CopySyms.empty()) { syms.push_back(CopySyms.top()); CopySyms.pop(); }
					while (!CopyVals.empty()) { vals.push_back(CopyVals.top()); CopyVals.pop(); }
					reverse(syms.begin(), syms.end());
					reverse(vals.begin(), vals.end());

					// 找最后一个 "if ( RelExpr ) Stmt" 模式
					int idx = -1;
					for (int i = (int)syms.size() - 5; i >= 0; --i) {
						if (syms[i].Name == "if" && syms[i + 1].Name == "(" && syms[i + 2].Name == "RelExpr" && syms[i + 3].Name == ")" && syms[i + 4].Name == "Stmt") {
							idx = i; break;
						}
					}
					if (idx != -1 && Is<BoolVal>(vals[idx + 2]) && Is<StmtVal>(vals[idx + 4])) {
						const auto& B = As<BoolVal>(vals[idx + 2]); // if 条件产生的 truelist/falselist
						const auto& S1 = As<StmtVal>(vals[idx + 4]); // then 语句入口 begin

						// then 执行完后要跳过 else：goto end（占位）
						int j = Emit("goto", "", "", "", -1);
						PendingIfElseEndJumps.push(j);

						// 条件真跳到 then 的入口
						Backpatch(B.truelist, S1.begin == -1 ? NextQuad() : S1.begin);

						// 条件假跳到 else 的入口：
						// 关键点：我们把 else 的入口定义为“goto end 之后的下一条”，即 NextQuad()
						Backpatch(B.falselist, NextQuad());
					}
				}

				/*
				(1) 函数头检测：在 SHIFT '(' 时尝试识别 “Type id ( ... ) { ... }” 的函数定义开始。
				为什么在 SHIFT '(' 做？
				- 当读到 '('，说明刚刚读过的 token 序列形如：Type id '('
				- 对你的语法来说：全局层面的 Type id '(' 最典型就是函数定义
				动作：
				- 将函数名插入全局符号表（kind=FUNC，type=返回类型）
				- 设置 PendingFunc/InFunction 等上下文，参数列表在后续归约 Parameter 时收集
				注意：
				- 这里用 Scopes.size()==1 限制为“全局层”识别函数，以免误判局部变量后的 '('（若语法扩展函数指针/调用等会更复杂）
				*/
				if (CurrentInput.Name == "(") {
					auto tmpSym = SymbolStack; auto tmpVal = ValueStack;
					GrammarSymbol s1 = tmpSym.top(); tmpSym.pop(); // 栈顶符号（刚移进的前一个符号）
					GrammarSymbol s2 = tmpSym.top(); // s1 下方符号

					SemVal v1 = tmpVal.top(); tmpVal.pop();
					SemVal v2 = tmpVal.top();

					if (s1.Name == "id" && s2.Name == "Type" && Is<IdVal>(v1) && Is<TypeVal>(v2) && Scopes.size() == 1) {
						auto idv = As<IdVal>(v1);
						auto tv = As<TypeVal>(v2);

						string err;
						Symbol fs; fs.kind = SymKind::FUNC; fs.type = tv.t; fs.irName = idv.name; fs.scopeLevel = 0;

						// 同一作用域重定义检查：同名函数重复定义直接报错
						if (!InsertHere(idv.name, fs, err)) {
							cout << "语义错误: " << err << " @ " << idv.pos << "\n";
							return false;
						}
						// 进入“函数定义上下文”，参数后面归约 Parameter 时累计到 PendingParams
						PendingFunc = true; InFunction = true;
						CurFuncName = idv.name; CurFuncRet = tv.t;
						PendingParams.clear();
					}
				}
#endif
				
				// 移进处理后的符号和状态
				SymbolStack.push(LookupSymbol);
				StateStack.push(Action.StateOrProduction);

#ifdef SEM_IR
				/*
				(3) 语义值入栈：与 SymbolStack 的 SHIFT 必须保持同步对齐。
				关键点：
				- 语法分析用于查 ACTION 的 LookupSymbol：ID/NUM 被统一成 "id"/"num"
				- 但语义分析需要保留原始 lexeme（例如变量名 "x"、常量 "123"）
				因此：语义值入栈使用 CurrentInput（原始 token），而不是 LookupSymbol。
				*/
				SemVal pushed = monostate{};
				if (CurrentInput.TokenType == "ID") {
					pushed = IdVal{ CurrentInput.Name,CurrentInput.Position };
				}
				else if (CurrentInput.TokenType == "NUM") {
					int x = 0; try { x = stoi(CurrentInput.Name); }
					catch (...) { x = 0; }
					pushed = NumVal{ x };
				}
				else if (CurrentInput.Name == "int") {
					pushed = TypeVal{ BaseType::INT };
				}
				else if (CurrentInput.Name == "void") {
					pushed = TypeVal{ BaseType::VOID };
				}
				else if (CurrentInput.Name == "<" || CurrentInput.Name == ">" || CurrentInput.Name == "<=" || CurrentInput.Name == ">=" || CurrentInput.Name == "==" || CurrentInput.Name == "!=") {
					// 关系运算符作为 OpVal 保存，后续在归约 RelExpr 时用来生成 if< / if== 等跳转
					pushed = OpVal{ CurrentInput.Name };
				}
				ValueStack.push(pushed);

				/*
				(4) 作用域管理：在 SHIFT '{' / SHIFT '}' 时维护符号表作用域栈。
				- '{'：BeginScope()，新建一个局部作用域（块作用域）
				- '}'：EndScope()，退出当前作用域
				并且：
				- 如果 PendingFunc=true，说明刚识别完函数头，但参数还没真正进入符号表
				  这里选择在进入函数体 '{' 时，把 PendingParams 插入到新建的作用域中（参数属于函数体最外层作用域）
				- FuncScopeDepth 用于判断函数体结束：当 EndScope 后 Scopes.size() < FuncScopeDepth 即认为函数结束
				*/
				if (CurrentInput.Name == "{") {
					BeginScope();
					// 若刚进入函数体：把参数插入到函数体作用域中
					if (PendingFunc) {
						FuncScopeDepth = (int)Scopes.size();
						// 参数名重名检查（同一个参数列表中不允许重复）
						set<string> seen;
						for (auto& [pname, pt] : PendingParams) {
							if (seen.count(pname)) { cout << "语义错误: 参数重名 " << pname << "\n"; return false; }
							seen.insert(pname);
							// 参数作为 PARAM 符号插入当前作用域，生成唯一名用于 IR
							Symbol ps; ps.kind = SymKind::PARAM; ps.type = pt; ps.irName = NewVarName(pname); ps.scopeLevel = (int)Scopes.size() - 1;
							string err;
							if (!InsertHere(pname, ps, err)) { cout << "语义错误: " << err << "\n"; return false; }
						}
						// 参数已落表，函数头处理完毕
						PendingFunc = false;
					}
				}
				else if (CurrentInput.Name == "}") {
					EndScope();
					// 若作用域弹出到了函数体之外，说明函数结束，清空函数上下文
					if (InFunction && (int)Scopes.size() < FuncScopeDepth) {
						// 函数体结束
						InFunction = false; CurFuncName.clear(); CurFuncRet = BaseType::ERR; FuncScopeDepth = 0;
					}
				}
#endif

				// 移动到下一个输入符号
				if (CurrentInput != EndSymbol)
				{
					InputIndex++;
				}
			}
			else if (Action.Type == SLRActionType::REDUCE)
			{
				cout << "执行归约操作: R" << Action.StateOrProduction << "\n";
				// 根据产生式ID获取产生式
				const Production& Prod = GetProductionById(Action.StateOrProduction);
				cout << "使用产生式: " << Prod.ToString() << "\n";

				// 如果是增广文法的开始产生式（Program' -> Program）
				if (Prod.Left.Name == Grammar.StartSymbol.Name)
				{
					cout << "\n分析成功：通过增广产生式接受\n";
#ifdef SEM_IR
					DumpSymbols();
					DumpIR();
#endif
					return true;
				}

#ifdef SEM_IR
				/*
				REDUCE 阶段的语义分析核心流程（LR/SLR 的标准写法）：
				- 设产生式 A -> β，|β|=n
				1) 从 ValueStack 弹出 n 个语义值，按从左到右存入 rhs[0..n-1]
				   （注意弹栈顺序是反的，所以这里用倒序写入）
				2) 与此同时，语法栈会弹出 n 个符号与 n 个状态（你在 SEM_IR 外已经做了）
				3) 执行“语义动作”：根据 rhs 计算 lhsVal（即 A 的综合属性）
				4) 归约完成后，把 lhsVal 再压回 ValueStack，与 SymbolStack 推入 A 保持对齐
				*/
				size_t n = Prod.Right.size();
				// 1) 收集 RHS 的语义值（从栈顶弹出 n 个）
				// rhs[i] 对应产生式右部第 i 个符号的语义值
				vector<SemVal> rhs(n);
				for (int i = (int)n - 1; i >= 0; --i) { rhs[i] = ValueStack.top(); ValueStack.pop(); }
#endif

				// 弹出栈中与产生式右部长度相等的状态和符号
				for (size_t i = 0; i < Prod.Right.size(); i++)
				{
					StateStack.pop();
					SymbolStack.pop();
				}

#ifdef SEM_IR
				/*
				lhsVal：当前归约产生式左部非终结符的语义值。
				下面这一长串 if/else 是“按非终结符名字 + 产生式形态”进行的语义动作分派：
				- 表达式/项/因子：做类型检查并 Emit 计算 IR，生成 ExprVal(place=临时变量/变量唯一名)
				- 关系表达式：Emit ifxxx/goto 占位，生成 BoolVal(truelist/falselist)
				- 声明/赋值：更新符号表 + Emit '='
				- if/while：Backpatch 回填控制流跳转
				- return：检查是否在函数内 + 返回类型匹配 + Emit ret/retv
				*/
				SemVal lhsVal = monostate{};

				auto& L = Prod.Left.Name;

				// ---- Type -> int/void 直接透传（shift 已压了 TypeVal） :contentReference[oaicite:4]{index=4}
				if (L == "Type") {
					if (Is<TypeVal>(rhs[0])) lhsVal = rhs[0];
					else if (Prod.Right[0].Name == "int") lhsVal = TypeVal{ BaseType::INT };
					else if (Prod.Right[0].Name == "void") lhsVal = TypeVal{ BaseType::VOID };
				}

				// ---- Parameter -> Type id：把参数先记到 PendingParams（等 '{' 再入作用域）:contentReference[oaicite:5]{index=5}
				else if (L == "Parameter") {
					auto tv = As<TypeVal>(rhs[0]);
					auto idv = As<IdVal>(rhs[1]);
					if (tv.t == BaseType::VOID) { cout << "语义错误: 参数不能是 void: " << idv.name << " @ " << idv.pos << "\n"; return false; }
					PendingParams.push_back({ idv.name,tv.t });
					lhsVal = monostate{};
				}

				// ---- Factor -> id/num/(Expr) :contentReference[oaicite:6]{index=6}
				else if (L == "Factor" && n == 1 && Prod.Right[0].Name == "id") {
					auto idv = As<IdVal>(rhs[0]);
					auto* sym = Lookup(idv.name);
					if (!sym) { cout << "语义错误: 使用未定义标识符 " << idv.name << " @ " << idv.pos << "\n"; return false; }
					if (sym->kind == SymKind::FUNC) { cout << "语义错误: 这里需要变量而不是函数 " << idv.name << " @ " << idv.pos << "\n"; return false; }
					lhsVal = ExprVal{ sym->type,sym->irName,-1 };
				}
				else if (L == "Factor" && n == 1 && Prod.Right[0].Name == "num") {
					auto nv = As<NumVal>(rhs[0]);
					lhsVal = ExprVal{ BaseType::INT,to_string(nv.v),-1 };
				}
				else if (L == "Factor" && n == 3 && Prod.Right[0].Name == "(") {
					lhsVal = rhs[1]; // ( Expr )
				}

				// ---- Term / Expr：算术类型检查 + 生成临时变量与 IR :contentReference[oaicite:7]{index=7}
				else if (L == "Term" && n == 3 && (Prod.Right[1].Name == "*" || Prod.Right[1].Name == "/")) {
					auto a = As<ExprVal>(rhs[0]), b = As<ExprVal>(rhs[2]);
					if (a.t != BaseType::INT || b.t != BaseType::INT) { cout << "语义错误: 乘除只支持 int\n"; return false; }
					string t = NewTemp();
					int idx = Emit(Prod.Right[1].Name, a.place, b.place, t);
					int bg = (a.begin != -1) ? a.begin : idx;
					lhsVal = ExprVal{ BaseType::INT,t,bg };
				}
				else if (L == "Term" && n == 1) {
					lhsVal = rhs[0];
				}
				else if (L == "Expr" && n == 3 && (Prod.Right[1].Name == "+" || Prod.Right[1].Name == "-")) {
					auto a = As<ExprVal>(rhs[0]), b = As<ExprVal>(rhs[2]);
					if (a.t != BaseType::INT || b.t != BaseType::INT) { cout << "语义错误: 加减只支持 int\n"; return false; }
					string t = NewTemp();
					int idx = Emit(Prod.Right[1].Name, a.place, b.place, t);
					int bg = (a.begin != -1) ? a.begin : idx;
					lhsVal = ExprVal{ BaseType::INT,t,bg };
				}
				else if (L == "Expr" && n == 1) {
					lhsVal = rhs[0];
				}

				// ---- RelOp：把操作符字符串做成 OpVal :contentReference[oaicite:8]{index=8}
				else if (L == "RelOp" && n == 1) {
					lhsVal = OpVal{ Prod.Right[0].Name };
				}

				// ---- RelExpr：生成 if-goto / goto 的占位并回填 :contentReference[oaicite:9]{index=9}
				else if (L == "RelExpr" && n == 3) {
					auto a = As<ExprVal>(rhs[0]);
					auto op = As<OpVal>(rhs[1]).op;
					auto b = As<ExprVal>(rhs[2]);
					if (a.t != BaseType::INT || b.t != BaseType::INT) { cout << "语义错误: 关系运算只支持 int\n"; return false; }
					int i = Emit("if" + op, a.place, b.place, "", -1);
					int j = Emit("goto", "", "", "", -1);
					lhsVal = BoolVal{ {i},{j},i };
				}
				else if (L == "RelExpr" && n == 1) {
					auto a = As<ExprVal>(rhs[0]);
					if (a.t != BaseType::INT) { cout << "语义错误: 条件表达式需要 int(非零为真)\n"; return false; }
					int i = Emit("ifnz", a.place, "", "", -1);
					int j = Emit("goto", "", "", "", -1);
					lhsVal = BoolVal{ {i},{j},i };
				}

				// ---- DeclarationStatement：插入符号表 + 可选初始化赋值 :contentReference[oaicite:10]{index=10}
				else if (L == "DeclarationStatement" && (n == 3 || n == 5)) {
					auto tv = As<TypeVal>(rhs[0]);
					auto idv = As<IdVal>(rhs[1]);
					if (tv.t == BaseType::VOID) { cout << "语义错误: 变量不能是 void: " << idv.name << " @ " << idv.pos << "\n"; return false; }

					Symbol vs; vs.kind = SymKind::VAR; vs.type = tv.t; vs.irName = NewVarName(idv.name); vs.scopeLevel = (int)Scopes.size() - 1;
					string err;
					if (!InsertHere(idv.name, vs, err)) { cout << "语义错误: " << err << " @ " << idv.pos << "\n"; return false; }

					int bg = NextQuad();
					if (n == 5) {
						auto e = As<ExprVal>(rhs[3]);
						if (e.t != tv.t) { cout << "语义错误: 初始化类型不匹配 " << idv.name << "\n"; return false; }
						int idx = Emit("=", e.place, "", vs.irName);
						bg = idx;
					}
					lhsVal = StmtVal{ {},bg };
				}

				// ---- AssignmentStatement：未定义/类型检查 + 生成赋值 IR :contentReference[oaicite:11]{index=11}
				else if (L == "AssignmentStatement" && n == 4) {
					auto idv = As<IdVal>(rhs[0]);
					auto* sym = Lookup(idv.name);
					if (!sym) { cout << "语义错误: 赋值给未定义标识符 " << idv.name << " @ " << idv.pos << "\n"; return false; }
					if (sym->kind == SymKind::FUNC) { cout << "语义错误: 不能给函数名赋值 " << idv.name << "\n"; return false; }
					auto e = As<ExprVal>(rhs[2]);
					if (e.t != sym->type) { cout << "语义错误: 赋值类型不匹配 " << idv.name << "\n"; return false; }
					int idx = Emit("=", e.place, "", sym->irName);
					lhsVal = StmtVal{ {},idx };
				}

				// ---- ExprStatement：Expr ; 或 ; :contentReference[oaicite:12]{index=12}
				else if (L == "ExprStatement" && n == 2) {
					auto e = As<ExprVal>(rhs[0]);
					lhsVal = StmtVal{ {}, e.begin != -1 ? e.begin : NextQuad() };
				}
				else if (L == "ExprStatement" && n == 1) {
					lhsVal = StmtVal{ {}, NextQuad() };
				}

				// ---- ReturnStatement：return/return Expr 类型检查 + emit return :contentReference[oaicite:13]{index=13}
				else if (L == "ReturnStatement" && (n == 2 || n == 3)) {
					if (!InFunction) { cout << "语义错误: return 只能出现在函数内\n"; return false; }
					int idx = NextQuad();
					if (n == 2) {
						if (CurFuncRet != BaseType::VOID) { cout << "语义错误: 非 void 函数必须 return 一个值\n"; return false; }
						idx = Emit("ret");
					}
					else {
						auto e = As<ExprVal>(rhs[1]);
						if (CurFuncRet != BaseType::INT || e.t != BaseType::INT) { cout << "语义错误: return 类型不匹配\n"; return false; }
						idx = Emit("retv", e.place);
					}
					lhsVal = StmtVal{ {},idx };
				}

				// ---- Stmt / StmtList：顺序连接时回填 nextlist :contentReference[oaicite:14]{index=14}
				else if (L == "Stmt") {
					lhsVal = rhs[0]; // 直接透传
				}
				else if (L == "StmtList" && n == 1) {
					lhsVal = rhs[0];
				}
				else if (L == "StmtList" && n == 2) {
					auto s1 = As<StmtVal>(rhs[0]);
					auto s2 = As<StmtVal>(rhs[1]);
					// s1 的“落空 nextlist”接到 s2 开始
					Backpatch(s1.nextlist, s2.begin == -1 ? NextQuad() : s2.begin);
					int bg = (s1.begin != -1) ? s1.begin : s2.begin;
					lhsVal = StmtVal{ s2.nextlist,bg };
				}

				// ---- CompoundStatement：{ } 或 { StmtList } :contentReference[oaicite:15]{index=15}
				else if (L == "CompoundStatement" && n == 2) {
					lhsVal = StmtVal{ {},NextQuad() };
				}
				else if (L == "CompoundStatement" && n == 3) {
					lhsVal = rhs[1];
				}

				// ---- SelectionStatement：if/if-else（if-else 的中间 goto 已在 shift else 做了） :contentReference[oaicite:16]{index=16}
				else if (L == "SelectionStatement" && n == 5) {
					auto B = As<BoolVal>(rhs[2]);
					auto S = As<StmtVal>(rhs[4]);
					Backpatch(B.truelist, S.begin == -1 ? NextQuad() : S.begin);
					Backpatch(B.falselist, NextQuad());
					Backpatch(S.nextlist, NextQuad());
					lhsVal = StmtVal{ {},B.begin };
				}
				else if (L == "SelectionStatement" && n == 7) {
					// if (B) S1 else S2：endJump 在 shift else 时 emit，这里只把它回填到 else 之后
					auto B = As<BoolVal>(rhs[2]);
					auto S1 = As<StmtVal>(rhs[4]);
					auto S2 = As<StmtVal>(rhs[6]);

					if (PendingIfElseEndJumps.empty()) { cout << "语义错误: if-else endJump 栈为空\n"; return false; }
					int j = PendingIfElseEndJumps.top(); PendingIfElseEndJumps.pop();
					Backpatch({ j }, NextQuad());

					Backpatch(S1.nextlist, NextQuad());
					Backpatch(S2.nextlist, NextQuad());
					lhsVal = StmtVal{ {},B.begin };
				}

				// ---- IterationStatement：while (B) S :contentReference[oaicite:17]{index=17}
				else if (L == "IterationStatement" && n == 5) {
					auto B = As<BoolVal>(rhs[2]);
					auto S = As<StmtVal>(rhs[4]);
					Backpatch(B.truelist, S.begin == -1 ? NextQuad() : S.begin);
					Backpatch(S.nextlist, B.begin);
					Emit("goto", "", "", "", B.begin);
					Backpatch(B.falselist, NextQuad());
					lhsVal = StmtVal{ {},B.begin };
				}

				// 其他非终结符（Program/GlobalDeclarations/FunctionDefinitions/FunctionDefinition/ParameterList等）
				// 最小化：先不额外做 IR，只透传或给空值
				else {
					// 常见透传：A->B
					if (n == 1) lhsVal = rhs[0];
					else lhsVal = monostate{};
				}
#endif

				// 获取归约后的当前状态
				int AfterReduceState = StateStack.top();

				// 压入产生式左部符号
				SymbolStack.push(Prod.Left);

				// 查找GOTO表，获取新状态
				int GotoState = TableBuilder.GetGoto(AfterReduceState, Prod.Left);
				if (GotoState == -1)
				{
					cout << "错误：在状态" << AfterReduceState << "对非终结符" << Prod.Left.Name << "的GOTO未找到\n";
					return false;
				}

				// 压入新状态
				StateStack.push(GotoState);
#ifdef SEM_IR
				ValueStack.push(lhsVal);
#endif
			}
			else if (Action.Type == SLRActionType::ACCEPT)
			{
				cout << "\n分析成功：ACCEPT\n";
				return true;
			}
			else
			{
				cout << "错误：在状态" << CurrentState << "对符号" << CurrentInput.Name << "的ACTION未找到\n";
				return false;
			}
		} // while loop
	}

	// 解析词法分析器输出文件
	bool ParseFromFile(const string& tokenFile)
	{
		// 从文件中读取tokens
		vector<GrammarSymbol> Tokens = LoadTokensFromFile(tokenFile);

		if (Tokens.empty())
		{
			cout << "未从文件中读取到有效的tokens" << endl;
			return false;
		}

		// 执行分析
		cout << "\n从文件中读取到的Tokens: ";
		for (const auto& symbol : Tokens)
		{
			cout << symbol.Name << " ";
		}
		cout << "\n";

		// 调用Parse函数进行解析
		return Parse(Tokens);
	}

	// 从词法分析器输出文件中读取tokens并转换为GrammarSymbol向量
	vector<GrammarSymbol> LoadTokensFromFile(const string& tokenFile)
	{
		vector<GrammarSymbol> Tokens;
		ifstream File(tokenFile);
		string Line;

		if (!File.is_open())
		{
			cerr << "无法打开词法分析器输出文件: " << tokenFile << endl;
			return Tokens;
		}

		while (getline(File, Line))
		{
			istringstream Iss(Line);
			string TokenTypeStr, Colon, TokenValue, Position;

			// 类型
			if (Iss >> TokenTypeStr)
			{
				// 处理ENDFILE
				if (TokenTypeStr == "ENDFILE")
				{
					Iss >> Position; // 位置
					continue;
				}

				// 读取冒号和token值
				if (Iss >> Colon && Colon == ":" && Iss >> TokenValue)
				{
					Iss >> Position;

					Tokens.push_back(GrammarSymbol(TokenValue, true, TokenTypeStr, Position));
				}
			}
		}

		File.close();
		return Tokens;
	}

};

#endif // SHIFTREDUCEPARSER_HPP
