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
#include <ctime>

using namespace std;

// 语义属性：存储临时变量名、标签等
struct SemanticAttribute
{
	string Value;  // 值（临时变量名、标签名、变量名等）
	string Type;   // 类型（"temp", "label", "var", "num", "stmt_start"等）
	int CodeStartPos; // 代码起始位置（用于Stmt）
	string Expr1;  // 表达式1（用于RelExpr）
	string Expr2;  // 表达式2（用于RelExpr）
	string Op;     // 运算符（用于RelExpr）

	SemanticAttribute(const string& val = "", const string& type = "", int codeStart = -1,
		const string& expr1 = "", const string& expr2 = "", const string& op = "")
		: Value(val), Type(type), CodeStartPos(codeStart), Expr1(expr1), Expr2(expr2), Op(op) {
	}
};

// 移进-归约分析器
struct ShiftReduceParser
{
	const SLRAnalysisTableBuilder& TableBuilder;
	const GrammarDefinition& Grammar;

	stack<int> StateStack;			  // 状态栈
	stack<GrammarSymbol> SymbolStack; // 符号栈
	stack<SemanticAttribute> SemanticStack; // 语义栈

	vector<string> ThreeAddressCode;  // 三地址码列表
	int TempVarCounter;               // 临时变量计数器
	int LabelCounter;                  // 标签计数器

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
		: TableBuilder(tableBuilder), Grammar(tableBuilder.Grammar),
		TempVarCounter(0), LabelCounter(0)
	{
		// 初始化状态栈和符号栈
		StateStack.push(0);							// 初始状态为0
		SymbolStack.push(GrammarSymbol("$", true)); // 栈底符号为$
		SemanticStack.push(SemanticAttribute("$", "marker")); // 语义栈底标记
	}

	// 生成新的临时变量名
	string NewTemp()
	{
		return "t" + to_string(TempVarCounter++);
	}

	// 生成新的标签名
	string NewLabel()
	{
		return "L" + to_string(LabelCounter++);
	}

	// 添加三地址码
	void EmitCode(const string& code)
	{
		if (!code.empty())
		{
			ThreeAddressCode.push_back(code);
		}
	}

	// 在指定位置插入代码
	void InsertCodeAt(size_t pos, const string& code)
	{
		if (!code.empty() && pos <= ThreeAddressCode.size())
		{
			ThreeAddressCode.insert(ThreeAddressCode.begin() + pos, code);
		}
	}

	// 生成三地址码：根据产生式ID
	// 注意：调用此函数时，语义栈中已经包含了产生式右部所有符号的属性（按从右到左的顺序）
	void GenerateCode(int prodId, const Production& prod)
	{
		// 表达式：Expr -> Expr + Term
		if (prod.Left.Name == "Expr" && prod.Right.size() == 3 && prod.Right[1].Name == "+")
		{
			SemanticAttribute term = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute op = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute expr = SemanticStack.top(); SemanticStack.pop();
			string temp = NewTemp();
			EmitCode(temp + " = " + expr.Value + " + " + term.Value);
			SemanticStack.push(SemanticAttribute(temp, "temp"));
		}
		// 表达式：Expr -> Expr - Term
		else if (prod.Left.Name == "Expr" && prod.Right.size() == 3 && prod.Right[1].Name == "-")
		{
			SemanticAttribute term = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute op = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute expr = SemanticStack.top(); SemanticStack.pop();
			string temp = NewTemp();
			EmitCode(temp + " = " + expr.Value + " - " + term.Value);
			SemanticStack.push(SemanticAttribute(temp, "temp"));
		}
		// 表达式：Expr -> Term
		else if (prod.Left.Name == "Expr" && prod.Right.size() == 1 && prod.Right[0].Name == "Term")
		{
			// 直接传递属性，语义栈不变
		}
		// 项：Term -> Term * Factor
		else if (prod.Left.Name == "Term" && prod.Right.size() == 3 && prod.Right[1].Name == "*")
		{
			SemanticAttribute factor = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute op = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute term = SemanticStack.top(); SemanticStack.pop();
			string temp = NewTemp();
			EmitCode(temp + " = " + term.Value + " * " + factor.Value);
			SemanticStack.push(SemanticAttribute(temp, "temp"));
		}
		// 项：Term -> Term / Factor
		else if (prod.Left.Name == "Term" && prod.Right.size() == 3 && prod.Right[1].Name == "/")
		{
			SemanticAttribute factor = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute op = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute term = SemanticStack.top(); SemanticStack.pop();
			string temp = NewTemp();
			EmitCode(temp + " = " + term.Value + " / " + factor.Value);
			SemanticStack.push(SemanticAttribute(temp, "temp"));
		}
		// 项：Term -> Factor
		else if (prod.Left.Name == "Term" && prod.Right.size() == 1 && prod.Right[0].Name == "Factor")
		{
			// 直接传递属性，语义栈不变
		}
		// 因子：Factor -> ( Expr )
		else if (prod.Left.Name == "Factor" && prod.Right.size() == 3 && prod.Right[0].Name == "(")
		{
			SemanticAttribute rparen = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute expr = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute lparen = SemanticStack.top(); SemanticStack.pop();
			// 直接传递Expr的属性
			SemanticStack.push(expr);
		}
		// 赋值语句：AssignmentStatement -> id = Expr ;
		else if (prod.Left.Name == "AssignmentStatement" && prod.Right.size() == 4 && prod.Right[1].Name == "=")
		{
			SemanticAttribute semicolon = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute expr = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute equals = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute id = SemanticStack.top(); SemanticStack.pop();
			size_t codeStart = ThreeAddressCode.size();
			EmitCode(id.Value + " = " + expr.Value);

			// 记录代码位置，用于Stmt
			SemanticStack.push(SemanticAttribute("", "stmt_start", codeStart));
		}
		// 声明语句：DeclarationStatement -> Type id = Expr ;
		else if (prod.Left.Name == "DeclarationStatement" && prod.Right.size() == 5 &&
			prod.Right[0].Name == "Type" && prod.Right[2].Name == "=")
		{
			SemanticAttribute semicolon = SemanticStack.top(); SemanticStack.pop(); // ;
			SemanticAttribute expr = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute equals = SemanticStack.top(); SemanticStack.pop(); // =
			SemanticAttribute id = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute type = SemanticStack.top(); SemanticStack.pop(); // Type

			size_t codeStart = ThreeAddressCode.size();
			EmitCode(id.Value + " = " + expr.Value);
			SemanticStack.push(SemanticAttribute("", "stmt_start", codeStart));
		}
		// 声明语句（无初始化）：DeclarationStatement -> Type id ;
		else if (prod.Left.Name == "DeclarationStatement" && prod.Right.size() == 3 &&
			prod.Right[0].Name == "Type" && prod.Right[2].Name == ";")
		{
			SemanticAttribute semicolon = SemanticStack.top(); SemanticStack.pop(); // ;
			SemanticAttribute id = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute type = SemanticStack.top(); SemanticStack.pop(); // Type

			// 不生成赋值代码，记录位置
			SemanticStack.push(SemanticAttribute("", "stmt_start", ThreeAddressCode.size()));
		}
		// RelOp -> { <, >, <=, >=, ==, != }
		else if (prod.Left.Name == "RelOp")
		{
			// 语义栈不变，运算符属性传递
		}
		// Rel表达式：RelExpr -> Expr RelOp Expr
		else if (prod.Left.Name == "RelExpr" && prod.Right.size() == 3 && prod.Right[1].Name == "RelOp")
		{
			SemanticAttribute expr2 = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute relOp = SemanticStack.top(); SemanticStack.pop();
			SemanticAttribute expr1 = SemanticStack.top(); SemanticStack.pop();
			string temp = NewTemp();

			size_t codeStart = ThreeAddressCode.size();
			EmitCode(temp + " = " + expr1.Value + " " + relOp.Value + " " + expr2.Value);

			SemanticStack.push(SemanticAttribute(temp, "temp", codeStart, expr1.Value, expr2.Value, relOp.Value));
		}
		// Rel表达式：RelExpr -> Expr
		else if (prod.Left.Name == "RelExpr" && prod.Right.size() == 1 && prod.Right[0].Name == "Expr")
		{
			SemanticAttribute expr = SemanticStack.top(); SemanticStack.pop();
			SemanticStack.push(expr);
		}
		// if语句：SelectionStatement -> if ( RelExpr ) Stmt
		else if (prod.Left.Name == "SelectionStatement" && prod.Right.size() == 5 && prod.Right[0].Name == "if")
		{
			// 产生式右部：if ( RelExpr ) Stmt
			// 从右到左弹出：Stmt, ), RelExpr, (, if
			// RelExpr是第3个（索引2，从右数第3个）
			SemanticAttribute relExpr("", "");
			SemanticAttribute stmt("", "");
			vector<SemanticAttribute> attrs;
			// 弹出所有5个属性
			for (int i = 0; i < 5; i++)
			{
				if (!SemanticStack.empty() && SemanticStack.top().Type != "marker")
				{
					attrs.push_back(SemanticStack.top());
					SemanticStack.pop();
				}
			}
			// 从右到左：attrs[0]=Stmt, attrs[1]=), attrs[2]=RelExpr, attrs[3]=(, attrs[4]=if
			if (attrs.size() > 2 && !attrs[2].Value.empty())
			{
				relExpr = attrs[2];
			}
			else
			{
				// 如果找不到，尝试找第一个非空属性
				for (const auto& attr : attrs)
				{
					if ((attr.Type == "temp" || attr.Type == "var" || attr.Type == "num") && !attr.Value.empty())
					{
						relExpr = attr;
						break;
					}
				}
			}
			// 获取Stmt的代码位置
			if (attrs.size() > 0)
			{
				stmt = attrs[0];
			}
			if (relExpr.Value.empty())
			{
				relExpr.Value = "0"; // 默认值
			}
			string falseLabel = NewLabel();
			// if语句的正确顺序：
			// 1. RelExpr代码（条件计算，已经在之前生成）
			// 2. if temp == 0 goto L1（条件检查，如果为假跳到结束）
			// 3. Stmt代码（if语句体）
			// 4. L1:（结束标签）

			// 找到RelExpr代码的结束位置和Stmt代码的起始位置
			size_t relExprEnd = 0;
			if (relExpr.CodeStartPos >= 0)
			{
				relExprEnd = relExpr.CodeStartPos + 1; // RelExpr代码只有一行
			}
			else
			{
				// 如果找不到CodeStartPos，查找包含relExpr.Value的代码行
				for (size_t i = 0; i < ThreeAddressCode.size(); i++)
				{
					if (ThreeAddressCode[i].find(relExpr.Value) != string::npos &&
						ThreeAddressCode[i].find("=") != string::npos)
					{
						relExprEnd = i + 1;
						break;
					}
				}
			}
			size_t stmtStart = (stmt.CodeStartPos >= 0 && stmt.Type == "stmt_start") ? stmt.CodeStartPos : ThreeAddressCode.size();

			// 在RelExpr代码之后、Stmt代码之前插入条件检查
			// 确保insertPos在正确的位置
			size_t insertPos = stmtStart;
			if (relExprEnd > 0 && relExprEnd < stmtStart)
			{
				insertPos = relExprEnd; // 在RelExpr代码之后
			}
			else if (stmtStart < ThreeAddressCode.size())
			{
				insertPos = stmtStart; // 在Stmt代码之前
			}
			InsertCodeAt(insertPos, "if " + relExpr.Value + " == 0 goto " + falseLabel);

			// 在Stmt代码之后插入结束标签
			EmitCode(falseLabel + ":");
		}
		// if-else语句：SelectionStatement -> if ( RelExpr ) Stmt else Stmt
		else if (prod.Left.Name == "SelectionStatement" && prod.Right.size() == 7 && prod.Right[0].Name == "if")
		{
			// 产生式右部：if ( RelExpr ) Stmt else Stmt
			// 从右到左弹出：Stmt2, else, Stmt1, ), RelExpr, (, if
			// RelExpr是第5个（索引4，从右数第5个）
			SemanticAttribute relExpr("", "");
			vector<SemanticAttribute> attrs;
			// 弹出所有7个属性
			for (int i = 0; i < 7; i++)
			{
				if (!SemanticStack.empty() && SemanticStack.top().Type != "marker")
				{
					attrs.push_back(SemanticStack.top());
					SemanticStack.pop();
				}
			}
			// 从右到左：attrs[0]=Stmt2, attrs[1]=else, attrs[2]=Stmt1, attrs[3]=), attrs[4]=RelExpr, attrs[5]=(, attrs[6]=if
			if (attrs.size() > 4 && !attrs[4].Value.empty())
			{
				relExpr = attrs[4];
			}
			else
			{
				// 如果找不到，尝试找第一个非空属性
				for (const auto& attr : attrs)
				{
					if ((attr.Type == "temp" || attr.Type == "var" || attr.Type == "num") && !attr.Value.empty())
					{
						relExpr = attr;
						break;
					}
				}
			}
			if (relExpr.Value.empty())
			{
				relExpr.Value = "0"; // 默认值
			}
			string falseLabel = NewLabel();
			string endLabel = NewLabel();
			EmitCode("if " + relExpr.Value + " == 0 goto " + falseLabel);
			EmitCode("goto " + endLabel);
			EmitCode(falseLabel + ":");
			EmitCode(endLabel + ":");
		}
		// while循环：IterationStatement -> while ( RelExpr ) Stmt
		else if (prod.Left.Name == "IterationStatement" && prod.Right.size() == 5 && prod.Right[0].Name == "while")
		{
			// 产生式右部：while ( RelExpr ) Stmt
			// 从右到左弹出：Stmt, ), RelExpr, (, while
			// RelExpr是第3个（索引2，从右数第3个）
			SemanticAttribute relExpr("", "");
			SemanticAttribute stmt("", "");
			vector<SemanticAttribute> attrs;
			// 弹出所有5个属性
			for (int i = 0; i < 5; i++)
			{
				if (!SemanticStack.empty() && SemanticStack.top().Type != "marker")
				{
					attrs.push_back(SemanticStack.top());
					SemanticStack.pop();
				}
			}
			// 从右到左：attrs[0]=Stmt, attrs[1]=), attrs[2]=RelExpr, attrs[3]=(, attrs[4]=while
			if (attrs.size() > 2 && !attrs[2].Value.empty())
			{
				relExpr = attrs[2];
			}
			else
			{
				// 如果找不到，尝试找第一个非空属性
				for (const auto& attr : attrs)
				{
					if ((attr.Type == "temp" || attr.Type == "var" || attr.Type == "num") && !attr.Value.empty())
					{
						relExpr = attr;
						break;
					}
				}
			}
			// 获取Stmt的代码位置
			if (attrs.size() > 0)
			{
				stmt = attrs[0];
			}
			if (relExpr.Value.empty())
			{
				relExpr.Value = "0"; // 默认值
			}

			// 生成标签
			string loopLabel = NewLabel();  //循环开始
			string endLabel = NewLabel();   //循环结束

			size_t stmtStart = 0;
			if (stmt.CodeStartPos >= 0 && stmt.Type == "stmt_start")
			{
				stmtStart = stmt.CodeStartPos;
				// 向前查找，找到循环体代码的真正起始位置
				// 循环体代码可能包括表达式计算的临时变量赋值，这些在stmt.CodeStartPos之前
				// 为了简化，默认循环体代码从stmt.CodeStartPos开始
			}
			else
			{
				// 如果找不到CodeStartPos，尝试从当前代码中查找Stmt代码的起始位置
				stmtStart = ThreeAddressCode.size();
			}

			// 由于RelExpr代码在循环外生成，我们需要找到RelExpr代码之后、Stmt代码之前的所有代码
			// 这些代码就是循环体代码（包括表达式计算的临时变量）
			size_t relExprEnd = 0;
			if (relExpr.CodeStartPos >= 0)
			{
				relExprEnd = relExpr.CodeStartPos + 1; // RelExpr代码只有一行
			}
			else
			{
				// 查找RelExpr代码的位置
				for (size_t i = 0; i < ThreeAddressCode.size(); i++)
				{
					if (ThreeAddressCode[i].find(relExpr.Value) != string::npos &&
						ThreeAddressCode[i].find("=") != string::npos)
					{
						relExprEnd = i + 1;
						break;
					}
				}
			}

			// 循环体代码的起始位置应该是RelExpr代码之后、Stmt代码之前的所有代码的起始位置
			// 如果stmtStart > relExprEnd，说明循环体代码在RelExpr之后
			// 循环体代码的真正起始位置应该是relExprEnd（RelExpr代码之后的第一行）
			if (stmtStart > relExprEnd)
			{
				stmtStart = relExprEnd; // 循环体代码从RelExpr代码之后开始
			}

			// 找到并删除原来的RelExpr代码（在循环外生成的，需要移到循环内）
			size_t relExprStart = relExprStart = relExpr.CodeStartPos;;

			// 如果RelExpr代码在Stmt代码之前，删除它并调整stmtStart
			if (relExprStart < stmtStart && relExprStart < ThreeAddressCode.size())
			{
				ThreeAddressCode.erase(ThreeAddressCode.begin() + relExprStart);
				if (stmtStart > relExprStart)
				{
					stmtStart--; // 调整stmtStart位置
				}
			}

			// 在Stmt代码之前插入：
			// 1. 循环开始标签 L0:（立即定位到循环开始位置）
			// 2. 重新生成RelExpr代码（在循环内部，每次循环都重新计算）
			// 3. 条件检查：if temp == 0 goto L1（如果条件为假，跳到循环结束）
			string codeAtStmtStart = (stmtStart < ThreeAddressCode.size()) ? ThreeAddressCode[stmtStart] : "N/A";
			InsertCodeAt(stmtStart, loopLabel + ":");

			// 重新生成RelExpr代码（使用保存的表达式信息）
			if (!relExpr.Expr1.empty() && !relExpr.Expr2.empty() && !relExpr.Op.empty())
			{
				string newTemp = NewTemp();
				InsertCodeAt(stmtStart + 1, newTemp + " = " + relExpr.Expr1 + " " + relExpr.Op + " " + relExpr.Expr2);
				InsertCodeAt(stmtStart + 2, "if " + newTemp + " == 0 goto " + endLabel);
			}
			else
			{
				// 如果没有保存表达式信息，使用原来的临时变量
				InsertCodeAt(stmtStart + 1, "if " + relExpr.Value + " == 0 goto " + endLabel);
			}

			// 在Stmt代码之后插入：
			// 4. goto L0（跳回循环开始）
			// 5. L1:（循环结束标签）
			size_t stmtEnd = ThreeAddressCode.size();
			InsertCodeAt(stmtEnd, "goto " + loopLabel);
			EmitCode(endLabel + ":");
		}
		// CompoundStatement -> { } 或 { StmtList }
		else if (prod.Left.Name == "CompoundStatement")
		{
			// 对于CompoundStatement归约，传递StmtList的代码位置
			vector<SemanticAttribute> attrs;
			for (size_t i = 0; i < prod.Right.size(); i++)
			{
				if (!SemanticStack.empty() && SemanticStack.top().Type != "marker")
				{
					attrs.push_back(SemanticStack.top());
					SemanticStack.pop();
				}
			}
			// 找到stmt_start类型的属性，记录代码位置
			int codeStart = -1;
			for (const auto& attr : attrs)
			{
				if (attr.Type == "stmt_start" && attr.CodeStartPos >= 0)
				{
					codeStart = attr.CodeStartPos;
					break;
				}
			}
			// 如果没有找到，使用当前代码位置
			if (codeStart < 0)
			{
				codeStart = ThreeAddressCode.size();
			}
			// 传递代码位置信息
			SemanticStack.push(SemanticAttribute("", "stmt_start", codeStart));
		}
		// StmtList -> Stmt 或 Stmt StmtList
		else if (prod.Left.Name == "StmtList")
		{
			// 对于StmtList归约，传递第一个Stmt的代码位置
			vector<SemanticAttribute> attrs;
			for (size_t i = 0; i < prod.Right.size(); i++)
			{
				if (!SemanticStack.empty() && SemanticStack.top().Type != "marker")
				{
					attrs.push_back(SemanticStack.top());
					SemanticStack.pop();
				}
			}
			// 找到stmt_start类型的属性，记录代码位置（第一个Stmt的位置）
			// 注意：对于 StmtList -> Stmt StmtList，attrs[0]=StmtList, attrs[1]=Stmt
			// 我们需要取左边的Stmt（attrs[1]），而不是attrs[0]（右边的StmtList）
			int codeStart = -1;
			if (prod.Right.size() == 2)
			{
				// StmtList -> Stmt StmtList：取左边的Stmt（attrs[1]）
				if (attrs.size() > 1 && attrs[1].Type == "stmt_start" && attrs[1].CodeStartPos >= 0)
				{
					codeStart = attrs[1].CodeStartPos;
				}
			}
			else
			{
				// StmtList -> Stmt：取Stmt（attrs[0]）
				if (attrs.size() > 0 && attrs[0].Type == "stmt_start" && attrs[0].CodeStartPos >= 0)
				{
					codeStart = attrs[0].CodeStartPos;
				}
			}
			// 如果没有找到，尝试从所有属性中查找
			if (codeStart < 0)
			{
				for (const auto& attr : attrs)
				{
					if (attr.Type == "stmt_start" && attr.CodeStartPos >= 0)
					{
						codeStart = attr.CodeStartPos;
						break;
					}
				}
			}
			// 如果还是没有找到，使用当前代码位置
			if (codeStart < 0)
			{
				codeStart = ThreeAddressCode.size();
			}
			// 传递代码位置信息
			SemanticStack.push(SemanticAttribute("", "stmt_start", codeStart));
		}
		// Stmt -> AssignmentStatement, CompoundStatement等
		else if (prod.Left.Name == "Stmt")
		{
			// 对于Stmt归约，记录代码位置
			vector<SemanticAttribute> attrs;
			for (size_t i = 0; i < prod.Right.size(); i++)
			{
				if (!SemanticStack.empty() && SemanticStack.top().Type != "marker")
				{
					attrs.push_back(SemanticStack.top());
					SemanticStack.pop();
				}
			}
			// 找到stmt_start类型的属性，记录代码位置
			int codeStart = -1;
			for (const auto& attr : attrs)
			{
				if (attr.Type == "stmt_start" && attr.CodeStartPos >= 0)
				{
					codeStart = attr.CodeStartPos;
					break;
				}
			}
			// 如果没有找到，使用当前代码位置
			if (codeStart < 0)
			{
				codeStart = ThreeAddressCode.size();
			}
			// 传递代码位置信息
			SemanticStack.push(SemanticAttribute("", "stmt_start", codeStart));
		}
		// 其他产生式：需要弹出对应数量的语义属性，但不生成代码
		else
		{
			// 对于不生成代码的产生式，需要弹出右部符号对应的语义属性
			// 但保留最后一个（如果有）作为左部的属性
			vector<SemanticAttribute> attrs;
			for (size_t i = 0; i < prod.Right.size(); i++)
			{
				if (!SemanticStack.empty() && SemanticStack.top().Type != "marker")
				{
					attrs.push_back(SemanticStack.top());
					SemanticStack.pop();
				}
			}
			// 如果有属性，传递第一个（通常是最后一个被压入的，即最左边的）
			if (!attrs.empty())
			{
				SemanticStack.push(attrs.back());
			}
		}
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
				// 移进处理后的符号和状态
				SymbolStack.push(LookupSymbol);
				StateStack.push(Action.StateOrProduction);

				// 处理语义栈：对于id和num，压入它们的值
				if (CurrentInput.TokenType == "ID" || CurrentInput.TokenType == "NUM")
				{
					SemanticStack.push(SemanticAttribute(CurrentInput.Name, CurrentInput.TokenType == "ID" ? "var" : "num"));
				}
				else if (CurrentInput.Name == "id" || CurrentInput.Name == "num")
				{
					// 如果已经是处理过的id或num，使用原始输入的值
					SemanticStack.push(SemanticAttribute(CurrentInput.Name, CurrentInput.Name == "id" ? "var" : "num"));
				}
				else if (CurrentInput.Name == "<" || CurrentInput.Name == ">" ||
					CurrentInput.Name == "<=" || CurrentInput.Name == ">=" ||
					CurrentInput.Name == "==" || CurrentInput.Name == "!=")
				{
					// 关系运算符
					SemanticStack.push(SemanticAttribute(CurrentInput.Name, "op"));
				}
				else
				{
					// 其他符号（运算符、括号等）压入空属性
					SemanticStack.push(SemanticAttribute("", "empty"));
				}

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
					return true;
				}

				// 生成三地址码（在弹出栈之前，因为需要访问语义栈）
				GenerateCode(Action.StateOrProduction, Prod);

				// 弹出栈中与产生式右部长度相等的状态和符号
				for (size_t i = 0; i < Prod.Right.size(); i++)
				{
					StateStack.pop();
					SymbolStack.pop();
				}

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
		}
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

	// 打印生成的三地址码
	void PrintThreeAddressCode() const
	{
		cout << "\n========== 生成的三地址码 ==========\n";
		if (ThreeAddressCode.empty())
		{
			cout << "未生成任何三地址码\n";
		}
		else
		{
			for (size_t i = 0; i < ThreeAddressCode.size(); i++)
			{
				cout << "[" << i << "] " << ThreeAddressCode[i] << "\n";
			}
		}
		cout << "===================================\n";
	}

	// 获取三地址码列表
	const vector<string>& GetThreeAddressCode() const
	{
		return ThreeAddressCode;
	}

};

#endif // SHIFTREDUCEPARSER_HPP