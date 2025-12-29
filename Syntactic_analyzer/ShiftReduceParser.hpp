#ifndef SHIFTREDUCEPARSER_HPP
#define SHIFTREDUCEPARSER_HPP

#include "GrammarLoader.hpp"
#include "SLRAnalysisTable.hpp"
#include "Token.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// 移进-归约分析器
struct ShiftReduceParser {
	const SLRAnalysisTableBuilder& TableBuilder;
	const GrammarDefinition& Grammar;

	stack<int> StateStack;           // 状态栈
	stack<GrammarSymbol> SymbolStack; // 符号栈

	// 辅助函数：根据产生式ID获取产生式
	const Production& GetProductionById(int prodId) const {
		for (const auto& prod : Grammar.Productions) {
			if (prod.Id == prodId) {
				return prod;
			}
		}
		throw runtime_error("未找到ID为" + to_string(prodId) + "的产生式");
	}

	// 辅助函数：打印当前栈状态
	void PrintStacks() const {
		// 打印状态栈
		cout << "状态栈: [ ";
		stack<int> tempStateStack = StateStack;
		vector<int> states;
		while (!tempStateStack.empty()) {
			states.push_back(tempStateStack.top());
			tempStateStack.pop();
		}
		for (auto it = states.rbegin(); it != states.rend(); ++it) {
			cout << *it << " ";
		}
		cout << "]\n";

		// 打印符号栈
		cout << "符号栈: [ ";
		stack<GrammarSymbol> tempSymbolStack = SymbolStack;
		vector<GrammarSymbol> symbols;
		while (!tempSymbolStack.empty()) {
			symbols.push_back(tempSymbolStack.top());
			tempSymbolStack.pop();
		}
		for (auto it = symbols.rbegin(); it != symbols.rend(); ++it) {
			cout << it->Name;
			if (!it->TokenType.empty()) {
				cout << "(" << it->TokenType << ")";
			}
			cout << " ";
		}
		cout << "]\n";
	}

	// 构造函数
	ShiftReduceParser(const SLRAnalysisTableBuilder& tableBuilder)
		: TableBuilder(tableBuilder), Grammar(tableBuilder.Grammar) {
		// 初始化状态栈和符号栈
		StateStack.push(0); // 初始状态为0
		SymbolStack.push(GrammarSymbol("$", true)); // 栈底符号为$
	}

	// 解析函数：接收符号序列，返回是否解析成功
	bool Parse(const vector<GrammarSymbol>& inputSymbols) {
		cout << "开始移进-归约分析...\n";

		size_t inputIndex = 0;
		const GrammarSymbol& endSymbol = TableBuilder.FFCalculator.EndSymbol;

		while (true) {
			// 获取当前状态和当前输入符号
			int currentState = StateStack.top();
			const GrammarSymbol& currentInput =
				(inputIndex < inputSymbols.size()) ? inputSymbols[inputIndex] : endSymbol;

			cout << "\n当前状态: " << currentState << ", 当前输入符号: " << currentInput.Name << "\n";
			PrintStacks();

			// 查找ACTION表
			const SLRAction& action = TableBuilder.GetAction(currentState, currentInput);

			if (action.Type == SLRActionType::SHIFT) {
				cout << "执行移进操作: S" << action.StateOrProduction << "\n";
				// 移进输入符号和状态
				SymbolStack.push(currentInput);
				StateStack.push(action.StateOrProduction);
				// 移动到下一个输入符号
				if (currentInput != endSymbol) {
					inputIndex++;
				}
			}
			else if (action.Type == SLRActionType::REDUCE) {
				cout << "执行归约操作: R" << action.StateOrProduction << "\n";
				// 根据产生式ID获取产生式
				const Production& prod = GetProductionById(action.StateOrProduction);
				cout << "使用产生式: " << prod.ToString() << "\n";

				// 特殊处理：如果是增广文法的开始产生式（Program' -> Program），直接接受
				if (prod.Left.Name == Grammar.StartSymbol.Name) {
					cout << "\n分析成功：通过增广产生式接受\n";
					return true;
				}

				// 弹出栈中与产生式右部长度相等的状态和符号
				for (size_t i = 0; i < prod.Right.size(); i++) {
					StateStack.pop();
					SymbolStack.pop();
				}

				// 获取归约后的当前状态
				int afterReduceState = StateStack.top();

				// 压入产生式左部符号
				SymbolStack.push(prod.Left);

				// 查找GOTO表，获取新状态
				int gotoState = TableBuilder.GetGoto(afterReduceState, prod.Left);
				if (gotoState == -1) {
					cout << "错误：在状态" << afterReduceState << "对非终结符" << prod.Left.Name << "的GOTO未找到\n";
					return false;
				}

				// 压入新状态
				StateStack.push(gotoState);
			}
			else if (action.Type == SLRActionType::ACCEPT) {
				cout << "\n分析成功：ACCEPT\n";
				return true;
			}
			else {
				cout << "错误：在状态" << currentState << "对符号" << currentInput.Name << "的ACTION未找到\n";
				return false;
			}
		}
	}

	// 解析词法分析器输出文件
	bool ParseFromFile(const string& tokenFile) {
		// 从文件中读取tokens
		vector<GrammarSymbol> Tokens = LoadTokensFromFile(tokenFile);

		if (Tokens.empty()) {
			cout << "未从文件中读取到有效的tokens" << endl;
			return false;
		}

		// 执行分析
		cout << "\n从文件中读取到的Tokens: ";
		for (const auto& symbol : Tokens) {
			cout << symbol.Name << " ";
		}
		cout << "\n";

		// 调用Parse函数进行解析
		return Parse(Tokens);
	}

	// 从词法分析器输出文件中读取tokens并转换为GrammarSymbol向量
	static vector<GrammarSymbol> LoadTokensFromFile(const string& tokenFile);
};

// 从词法分析器输出文件中读取tokens并转换为GrammarSymbol向量
vector<GrammarSymbol> ShiftReduceParser::LoadTokensFromFile(const string& tokenFile) {
	vector<GrammarSymbol> Tokens;
	ifstream File(tokenFile);
	string Line;

	if (!File.is_open()) {
		cerr << "无法打开词法分析器输出文件: " << tokenFile << endl;
		return Tokens;
	}

	while (getline(File, Line)) {
		istringstream iss(Line);
		string tokenType, colon, tempValue;

		iss >> tokenType;
		if (tokenType == "ENDFILE") {
			// 跳过ENDFILE标记
			continue;
		}

		iss >> colon; // 跳过冒号

		// 获取token值（直到遇到左括号）
		getline(iss, tempValue);

		// 去除前后空格
		size_t start = tempValue.find_first_not_of(" ");
		size_t end = tempValue.find_last_not_of(" ");
		if (start != string::npos && end != string::npos) {
			tempValue = tempValue.substr(start, end - start + 1);
		}

		// 提取token值，去除位置信息（左括号之前的部分）
		string tokenValue = tempValue;
		size_t pos = tempValue.find('(');
		if (pos != string::npos) {
			tokenValue = tempValue.substr(0, pos - 1); // -1去除括号前的空格
		}

		// 定义token类型到语法符号的映射表
		unordered_map<string, string> tokenToSymbolMap = {
			// 关键字
			{"READ", "read"},
			{"WRITE", "write"},
			{"IF", "if"},
			{"ELSE", "else"},
			{"WHILE", "while"},
			{"RETURN", "return"},
			{"INT", "int"},
			{"VOID", "void"},

			// 运算符
			{"PLUS", "+"},
			{"MINUS", "-"},
			{"MULT", "*"},
			{"DIV", "/"},
			{"GT", ">"},
			{"LT", "<"},
			{"GTE", ">="},
			{"LTE", "<="},
			{"EQ", "=="},
			{"NEQ", "!="},
			{"ASSIGN", "="},

			// 界符
			{"LPAREN", "("},
			{"RPAREN", ")"},
			{"LBRACE", "{"},
			{"RBRACE", "}"},
			{"SEMI", ";"},
			{"COMMA", ","}
		};

		// 处理token类型，映射到语法终端符号
		if (tokenType == "ID") {
			// ID类型映射到语法中的'id'终结符
			Tokens.push_back(GrammarSymbol("id", true, tokenType));
		}
		else if (tokenType == "NUM") {
			// NUM类型映射到语法中的'num'终结符
			Tokens.push_back(GrammarSymbol("num", true, tokenType));
		}
		else {
			// 查找映射表
			auto it = tokenToSymbolMap.find(tokenType);
			if (it != tokenToSymbolMap.end()) {
				// 找到映射，使用映射后的符号
				Tokens.push_back(GrammarSymbol(it->second, true, tokenType));
			}
			else {
				// 未找到映射，使用解析得到的token值
				Tokens.push_back(GrammarSymbol(tokenValue, true, tokenType));
			}
		}
	}

	File.close();
	return Tokens;
}

#endif // SHIFTREDUCEPARSER_HPP
