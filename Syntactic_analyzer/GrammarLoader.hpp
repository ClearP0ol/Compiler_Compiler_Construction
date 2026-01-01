#ifndef GRAMMARLOADER_HPP
#define GRAMMARLOADER_HPP

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// 语法符号（终结符或非终结符）
struct GrammarSymbol
{
	string Name;	  // 符号名
	bool IsTerminal;  // 是否为终结符
	string TokenType; // Token类型
	string Position;  // 位置

	// 构造函数
	GrammarSymbol(const string& name = "", bool isTerminal = false, const string& tokenType = "", const string& position = "")
		: Name(name), IsTerminal(isTerminal), TokenType(tokenType), Position(position)
	{
	}

	// 重载==运算符
	bool operator==(const GrammarSymbol& other) const
	{
		return Name == other.Name && IsTerminal == other.IsTerminal;
	}

	// 便于map使用的<运算符
	bool operator<(const GrammarSymbol& other) const
	{
		// 先按名称排序，如果名称相同再按类型排序
		if (Name != other.Name)
		{
			return Name < other.Name;
		}
		return IsTerminal < other.IsTerminal;
	}

	// 重载!=运算符
	bool operator!=(const GrammarSymbol& other) const
	{
		return !(*this == other);
	}
};

// 产生式
struct Production
{
	GrammarSymbol Left;			 // 左部非终结符
	vector<GrammarSymbol> Right; // 右部符号序列
	int Id;						 // 产生式编号

	// 空构造
	Production() : Id(-1) {}

	// 输入构造
	Production(const GrammarSymbol& left, const vector<GrammarSymbol>& right, int id = -1)
		: Left(left), Right(right), Id(id)
	{
	}

	// 输出为字符串
	string ToString() const
	{
		string Result = Left.Name + " -> ";
		for (const auto& Symbol : Right)
		{
			Result += Symbol.Name + " ";
		}
		return Result;
	}

	// 是否为空
	bool IsEpsilon() const
	{
		return Right.empty() || (Right.size() == 1 && Right[0].Name == "ε");
	}
};

// 语法定义
struct GrammarDefinition
{
	string Name;						// 语法名称
	GrammarSymbol StartSymbol;			// 开始符号
	vector<GrammarSymbol> Terminals;	// 终结符集合
	vector<GrammarSymbol> NonTerminals; // 非终结符集合
	vector<Production> Productions;		// 产生式集合

	// 查找符号
	GrammarSymbol FindSymbol(const string& name, bool isTerminal) const
	{
		// 确定集合
		vector<GrammarSymbol> Symbols;
		if (isTerminal)
		{
			Symbols = Terminals;
		}
		else
		{
			Symbols = NonTerminals;
		}
		// 查找
		for (const auto& Symbol : Symbols)
		{
			if (Symbol.Name == name && Symbol.IsTerminal == isTerminal)
			{
				return Symbol;
			}
		}
		// 未找到
		return GrammarSymbol("", isTerminal);
	}

	// 判断是否为终结符
	bool IsTerminal(const string& name) const
	{
		for (const auto& Symbol : Terminals)
		{
			if (Symbol.Name == name)
				return true;
		}
		return false;
	}

	// 判断是否为非终结符
	bool IsNonTerminal(const string& name) const
	{
		for (const auto& Symbol : NonTerminals)
		{
			if (Symbol.Name == name)
				return true;
		}
		return false;
	}

	// 获取某个左部的所有产生式
	vector<Production> GetProductionsByLeft(const string& leftName) const
	{
		vector<Production> Result;
		for (const auto& Production : Productions)
		{
			if (Production.Left.Name == leftName)
			{
				Result.push_back(Production);
			}
		}
		return Result;
	}
};

// 语法文件加载器
struct GrammarLoader
{
	GrammarLoader() = default;

	// 从文件加载语法
	GrammarDefinition LoadFromFile(const string& filePath)
	{

		GrammarDefinition Grammar;

		if (filePath.empty())
		{
			cout << "错误：文件路径为空" << endl;
			return Grammar;
		}

		ifstream File(filePath);

		if (!File.is_open())
		{
			cout << "语法文件打开失败：" << filePath << endl;
			return Grammar;
		}

		cout << "正在加载语法文件: " << filePath << endl;

		string Line;	 // 按行读取
		int LineNum = 0; // 行计数

		while (getline(File, Line))
		{
			LineNum++;
			Trim(Line); // 去除字符串首尾空白

			// 跳过空行和注释
			if (Line.empty() || Line[0] == '#')
			{
				continue;
			}

			// 解析语法名称
			if (Line.find("GRAMMAR_NAME") == 0)
			{
				Grammar.Name = ExtractValue(Line);
				continue;
			}

			// 解析开始符号
			if (Line.find("START_SYMBOL") == 0)
			{
				string StartSymbol = ExtractValue(Line);
				Grammar.StartSymbol = GrammarSymbol(StartSymbol, false);
				continue;
			}

			// 开始解析产生式
			if (Line.find("->") != string::npos)
			{
				ParseProduction(Line, Grammar, LineNum);
			}
			else
			{
				// 可能是一行只包含一个右部（续行）
				ParseRightPartOnly(Line, Grammar, LineNum);
			}
		}

		File.close();

		// 自动收集终结符和非终结符
		CollectSymbols(Grammar);

		// 为产生式编号
		for (size_t i = 0; i < Grammar.Productions.size(); i++)
		{
			Grammar.Productions[i].Id = static_cast<int>(i);
		}

		PrintGrammarSummary(Grammar);
		return Grammar;
	}

	// 去除字符串首尾空白
	static void Trim(string& str)
	{
		str.erase(0, str.find_first_not_of(" \t\r\n"));
		str.erase(str.find_last_not_of(" \t\r\n") + 1);
	}

	// 提取值
	static string ExtractValue(const string& line)
	{
		size_t SpacePos = line.find(' ');
		if (SpacePos == string::npos)
			return "";

		string Value = line.substr(SpacePos + 1);
		Trim(Value);
		return Value;
	}

	// 解析产生式
	void ParseProduction(const string& line, GrammarDefinition& grammar, int lineNum)
	{
		size_t ArrowPos = line.find("->");
		if (ArrowPos == string::npos)
		{
			cout << "error：第 " << lineNum << " 行没有找到 '->' 符号" << endl;
			return;
		}

		// 提取左部
		string LeftStr = line.substr(0, ArrowPos);
		Trim(LeftStr);

		// 提取右部
		string RightStr = line.substr(ArrowPos + 2);
		Trim(RightStr);

		// 创建产生式
		Production Prod;
		Prod.Left = GrammarSymbol(LeftStr, false);

		// 解析右部符号
		ParseRightSymbols(RightStr, Prod.Right);

		grammar.Productions.push_back(Prod);
	}

	// 解析只有右部的情况（续行）
	void ParseRightPartOnly(const string& line, GrammarDefinition& grammar, int lineNum)
	{
		if (grammar.Productions.empty())
		{
			cout << "error：第 " << lineNum << " 行没有对应的左部" << endl;
			return;
		}

		Production& LastProd = grammar.Productions.back();
		ParseRightSymbols(line, LastProd.Right);
	}

	// 解析右部符号
	void ParseRightSymbols(const string& rightStr, vector<GrammarSymbol>& symbols)
	{
		istringstream Iss(rightStr);
		string SymbolName;

		while (Iss >> SymbolName)
		{
			// 判断符号类型
			bool IsTerminal = IsTerminalSymbol(SymbolName);
			symbols.push_back(GrammarSymbol(SymbolName, IsTerminal));
		}
	}

	// 判断是否为终结符符号
	bool IsTerminalSymbol(const string& symbol)
	{
		// 运算符集合
		static const vector<string> TerminalKeywords = {
			// 单字符运算符
			"+", "-", "*", "/", "(", ")", "{", "}", ";", "=",
			"<", ">", "!", ",", ".", "&", "|", "^", "~", "%", "?",
			":", "[", "]",
			// 多字符运算符
			"==", "!=", "<=", ">=", ":=", "++", "--",
			"*=", "/=", "%=", "&=", "|=", "^=",
			"<<", ">>", "<<=", ">>=",
			"&&", "||", "->"
		};

		// 检查是否在运算符集合中
		for (const auto& Keyword : TerminalKeywords)
		{
			if (symbol == Keyword)
			{
				return true;
			}
		}

		// 字母开头的全小写的符号是终结符
		if (!symbol.empty() && isalpha(symbol[0]))
		{
			for (char c : symbol)
			{
				if (isalpha(c) && !islower(c))
				{
					return false; // 有大写字母，是非终结符
				}
			}
			return true; // 全小写，是终结符
		}

		return false;
	}

	// 添加符号到相应集合
	void AddSymbolIfNotExists(const GrammarSymbol& sym, GrammarDefinition& grammar)
	{
		// 如果为空
		if (sym.Name.empty())
			return;

		// 如果已经添加
		for (const auto& Existing : grammar.Terminals)
		{
			if (Existing.Name == sym.Name)
				return;
		}
		for (const auto& Existing : grammar.NonTerminals)
		{
			if (Existing.Name == sym.Name)
				return;
		}

		if (sym.IsTerminal)
		{
			grammar.Terminals.push_back(sym);
		}
		else
		{
			grammar.NonTerminals.push_back(sym);
		}
	}

	// 收集所有符号
	void CollectSymbols(GrammarDefinition& grammar)
	{

		// 首先添加起始符号为非终结符
		if (!grammar.StartSymbol.Name.empty())
		{
			grammar.NonTerminals.push_back(grammar.StartSymbol);
		}

		// 从产生式中收集符号
		for (const auto& prod : grammar.Productions)
		{
			// 左部符号
			AddSymbolIfNotExists(prod.Left, grammar);

			// 右部符号
			for (const auto& sym : prod.Right)
			{
				AddSymbolIfNotExists(sym, grammar);
			}
		}
	}

	// 打印语法摘要
	void PrintGrammarSummary(const GrammarDefinition& grammar)
	{
		cout << endl
			<< "语法加载完成。" << endl;
		cout << "语法名称: " << grammar.Name << endl;
		cout << "开始符号: " << grammar.StartSymbol.Name << endl;
		cout << "非终结符 (" << grammar.NonTerminals.size() << " 个): ";
		for (const auto& nt : grammar.NonTerminals)
		{
			cout << nt.Name << " ";
		}
		cout << endl
			<< "终结符 (" << grammar.Terminals.size() << " 个): ";
		for (const auto& t : grammar.Terminals)
		{
			cout << t.Name << " ";
		}
		cout << endl
			<< "产生式 (" << grammar.Productions.size() << " 个):" << endl;

		for (size_t i = 0; i < grammar.Productions.size(); i++)
		{
			cout << "[" << i << "] " << grammar.Productions[i].ToString() << endl;
		}
	}
};

#endif // GRAMMARLOADER_HPP
