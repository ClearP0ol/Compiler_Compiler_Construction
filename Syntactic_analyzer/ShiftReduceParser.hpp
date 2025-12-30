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

using namespace std;

// 移进-归约分析器
struct ShiftReduceParser
{
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
	}

	// 解析函数：接收符号序列，返回是否解析成功
	bool Parse(const vector<GrammarSymbol>& inputSymbols)
	{
		cout << "开始移进-归约分析...\n";

		size_t InputIndex = 0;
		const GrammarSymbol& EndSymbol = TableBuilder.FFCalculator.EndSymbol;

		while (true)
		{
			// 获取当前状态和当前输入符号
			int CurrentState = StateStack.top();
			const GrammarSymbol& CurrentInput =
				(InputIndex < inputSymbols.size()) ? inputSymbols[InputIndex] : EndSymbol;

			cout << "\n当前状态: " << CurrentState << ", 当前输入符号: " << CurrentInput.Name << "\n";
			PrintStacks();

			// 查找ACTION表
			const SLRAction& Action = TableBuilder.GetAction(CurrentState, CurrentInput);

			if (Action.Type == SLRActionType::SHIFT)
			{
				cout << "执行移进操作: S" << Action.StateOrProduction << "\n";
				// 移进输入符号和状态
				SymbolStack.push(CurrentInput);
				StateStack.push(Action.StateOrProduction);
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

					// 特殊处理ID和NUM类型
					if (TokenTypeStr == "ID")
					{
						Tokens.push_back(GrammarSymbol("id", true, TokenTypeStr, Position));
					}
					else if (TokenTypeStr == "NUM")
					{
						Tokens.push_back(GrammarSymbol("num", true, TokenTypeStr, Position));
					}
					else
					{
						// 其他类型直接使用token值
						Tokens.push_back(GrammarSymbol(TokenValue, true, TokenTypeStr, Position));
					}
				}
			}
		}

		File.close();
		return Tokens;
	}
};

#endif // SHIFTREDUCEPARSER_HPP
