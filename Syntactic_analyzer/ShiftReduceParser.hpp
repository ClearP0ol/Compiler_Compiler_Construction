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
	enum class BaseType { INT, VOID, BOOL, ERR };

	static inline const char* TypeName(BaseType t) {
		switch (t) {
			case BaseType::INT:return "int";
			case BaseType::VOID:return "void";
			case BaseType::BOOL:return "bool";
			default:return "err";
		}
	}

	// ====== 三地址/四元式 IR ======
	struct Quad {
		string op, a1, a2, res;
		int target = -1; // goto/if 的目标，-1 表示占位
	};
	vector<Quad> IR;

	int NextQuad()const { return (int)IR.size(); }
	int Emit(const string& op, const string& a1 = "", const string& a2 = "", const string& res = "", int target = -1) {
		IR.push_back({ op,a1,a2,res,target });
		return (int)IR.size() - 1;
	}
	static inline vector<int> Merge(const vector<int>& a, const vector<int>& b) {
		vector<int> r = a; r.insert(r.end(), b.begin(), b.end()); return r;
	}
	void Backpatch(const vector<int>& lst, int target) {
		for (int i : lst) {
			if (i < 0 || i >= (int)IR.size())continue;
			IR[i].target = target;
		}
	}

	// ====== 语义值（按需最小集）======
	struct TypeVal { BaseType t = BaseType::ERR; };
	struct IdVal { string name, pos; };         // 原始标识符文本 + 位置信息
	struct NumVal { int v = 0; };

	struct ExprVal { BaseType t = BaseType::ERR; string place; int begin = -1; }; // place=变量/临时/常量文本
	struct BoolVal { vector<int> truelist, falselist; int begin = -1; };
	struct StmtVal { vector<int> nextlist; int begin = -1; };

	struct OpVal { string op; };               // RelOp

	using SemVal = variant<monostate, TypeVal, IdVal, NumVal, ExprVal, BoolVal, StmtVal, OpVal>;
	stack<SemVal> ValueStack;

	// 方便取 variant
	template<class T>static inline const T& As(const SemVal& v) { return get<T>(v); }
	template<class T>static inline bool Is(const SemVal& v) { return holds_alternative<T>(v); }

	// ====== 符号表（作用域栈）======
	enum class SymKind { VAR, FUNC, PARAM };

	struct Symbol {
		SymKind kind = SymKind::VAR;
		BaseType type = BaseType::ERR;           // var/param 的类型；func 的返回类型
		vector<BaseType> params;               // func 参数类型
		string irName;                         // 变量唯一名（避免同名遮蔽混淆）
		int scopeLevel = 0;
	};

	vector<unordered_map<string, Symbol>> Scopes;
	int uniqId = 0;

	void BeginScope() { Scopes.push_back({}); }
	void EndScope() {
		if (Scopes.size() > 1)Scopes.pop_back(); // 留一个全局 scope
	}

	Symbol* Lookup(const string& name) {
		for (int i = (int)Scopes.size() - 1; i >= 0; --i) {
			auto it = Scopes[i].find(name);
			if (it != Scopes[i].end())return &it->second;
		}
		return nullptr;
	}

	bool InsertHere(const string& name, const Symbol& sym, string& err) {
		auto& cur = Scopes.back();
		if (cur.find(name) != cur.end()) {
			err = "重定义标识符: " + name;
			return false;
		}
		cur[name] = sym;
		return true;
	}

	string NewTemp() { return "t" + to_string(++uniqId); }
	string NewVarName(const string& raw) { return raw + "@" + to_string((int)Scopes.size() - 1) + "#" + to_string(++uniqId); }

	// ====== 函数上下文（最小化）======
	bool PendingFunc = false;
	bool InFunction = false;
	string CurFuncName;
	BaseType CurFuncRet = BaseType::ERR;
	int FuncScopeDepth = 0;
	vector<pair<string, BaseType>> PendingParams;

	stack<int> PendingIfElseEndJumps; // 处理 if-else：在 shift else 时 emit 的 “goto end” 占位

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
		ValueStack.push(monostate{});
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
				// ====== 0) 处理 shift 前置动作：else 需要在“进入 else 语句前”插入 goto end，并回填条件 false 跳转 ======
				if (CurrentInput.Name == "else") {
					// 栈里此时应形如：... if ( RelExpr ) Stmt   （else 还没入栈）
					// 取最近的 RelExpr 和 then Stmt：直接复制栈到数组做模式匹配（最小化实现）
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
						const auto& B = As<BoolVal>(vals[idx + 2]);
						const auto& S1 = As<StmtVal>(vals[idx + 4]);

						// 1) then 结束后跳过 else：emit goto _
						int j = Emit("goto", "", "", "", -1);
						PendingIfElseEndJumps.push(j);

						// 2) 条件真跳到 then 开始
						Backpatch(B.truelist, S1.begin == -1 ? NextQuad() : S1.begin);

						// 3) 条件假跳到 else 开始（注意：else 开始应在 goto 之后）
						Backpatch(B.falselist, NextQuad()); // NextQuad() 此刻正好是 else 第一条 IR 的位置
					}
				}

				// ====== 1) 函数头检测：看到 '(' 且栈顶是 id，下面是 Type => 函数定义开始（语法里只有函数定义会这样）:contentReference[oaicite:2]{index=2}
				if (CurrentInput.Name == "(") {
					auto tmpSym = SymbolStack; auto tmpVal = ValueStack;
					GrammarSymbol s1 = tmpSym.top(); tmpSym.pop();
					GrammarSymbol s2 = tmpSym.top();

					SemVal v1 = tmpVal.top(); tmpVal.pop();
					SemVal v2 = tmpVal.top();

					if (s1.Name == "id" && s2.Name == "Type" && Is<IdVal>(v1) && Is<TypeVal>(v2) && Scopes.size() == 1) {
						auto idv = As<IdVal>(v1);
						auto tv = As<TypeVal>(v2);

						string err;
						Symbol fs; fs.kind = SymKind::FUNC; fs.type = tv.t; fs.irName = idv.name; fs.scopeLevel = 0;
						if (!InsertHere(idv.name, fs, err)) {
							cout << "语义错误: " << err << " @ " << idv.pos << "\n";
							return false;
						}
						PendingFunc = true; InFunction = true;
						CurFuncName = idv.name; CurFuncRet = tv.t;
						PendingParams.clear();
						// 你也可以 Emit 一个 func label：Emit("func",CurFuncName,"","")
					}
				}
#endif
				
				// 移进处理后的符号和状态
				SymbolStack.push(LookupSymbol);
				StateStack.push(Action.StateOrProduction);

#ifdef SEM_IR
				// ====== 3) 语义值入栈（注意：用 CurrentInput 保留 lexeme）======
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
					pushed = OpVal{ CurrentInput.Name };
				}
				ValueStack.push(pushed);

				// ====== 4) 作用域管理：{ 开新作用域；} 关作用域（最小化）:contentReference[oaicite:3]{index=3}
				if (CurrentInput.Name == "{") {
					BeginScope();
					if (PendingFunc) {
						FuncScopeDepth = (int)Scopes.size();
						// 把参数真正插入到函数体作用域
						set<string> seen;
						for (auto& [pname, pt] : PendingParams) {
							if (seen.count(pname)) { cout << "语义错误: 参数重名 " << pname << "\n"; return false; }
							seen.insert(pname);
							Symbol ps; ps.kind = SymKind::PARAM; ps.type = pt; ps.irName = NewVarName(pname); ps.scopeLevel = (int)Scopes.size() - 1;
							string err;
							if (!InsertHere(pname, ps, err)) { cout << "语义错误: " << err << "\n"; return false; }
						}
						PendingFunc = false;
					}
				}
				else if (CurrentInput.Name == "}") {
					EndScope();
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
				size_t n = Prod.Right.size();
				// 1) 收集 RHS 的语义值（从栈顶弹出 n 个）
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
				SemVal lhsVal = monostate{};

				auto L = Prod.Left.Name;

				// ---- Type -> int/void 直接透传（你 shift 已压了 TypeVal） :contentReference[oaicite:4]{index=4}
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
