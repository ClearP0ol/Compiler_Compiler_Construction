#include "FirstFollowCalculator.hpp"
#include "LRAutomaton.hpp"
#include "ShiftReduceParser.hpp"
#include "SLRAnalysisTable.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

int main()
{
	// 创建语法加载器
	GrammarLoader Loader;

	// 测试文件路径
	string GrammarFile = "MiniC.grammar";  // 切换到MiniC语法文件
	// string GrammarFile = "Tiny.grammar";   // 切换到Tiny语法文件
	// string GrammarFile = "Expr.grammar";   // 切换到Expr语法文件

	// 加载语法
	GrammarDefinition Grammar = Loader.LoadFromFile(GrammarFile);

	// 检查是否加载成功
	if (Grammar.Productions.empty())
	{
		cout << endl
			 << "语法加载失败！" << endl;
		return 1;
	}

	cout << endl
		 << "语法加载成功！" << endl;

	// 创建FIRST/FOLLOW计算器
	cout << "\n计算FIRST和FOLLOW集合。" << endl;
	FirstFollowCalculator Calculator(Grammar);

	// 计算FIRST和FOLLOW集合
	Calculator.Calculate();

	// 打印FIRST集合
	Calculator.PrintFirstSets();

	// 打印FOLLOW集合
	Calculator.PrintFollowSets();

	// 测试LR(0)自动机构建器
	cout << "\n测试LR(0)自动机构建器" << endl;

	try
	{
		// 创建LR(0)自动机
		LRAutomatonBuilder AutomatonBuilder(Grammar);

		// 打印自动机信息
		cout << "LR(0)自动机构建成功！" << endl;
		cout << "自动机状态数量: " << AutomatonBuilder.States.size() << endl;

		// 打印增广语法信息
		cout << "增广语法开始符号: " << AutomatonBuilder.AugmentedGrammar.StartSymbol.Name << endl;

		// 打印所有状态
		AutomatonBuilder.PrintAutomaton();

		// 测试SLR(1)分析表生成器
		cout << "\n测试SLR(1)分析表生成器" << endl;

		// 为增广文法创建FIRST/FOLLOW计算器
		FirstFollowCalculator AugmentedFF(AutomatonBuilder.AugmentedGrammar);
		AugmentedFF.Calculate();

		// 创建SLR分析表生成器
		SLRAnalysisTableBuilder SLRTable(AutomatonBuilder, AugmentedFF);

		// 打印分析表
		cout << "SLR(1)分析表生成成功！" << endl;
		SLRTable.PrintTable();

		// 测试移进-归约分析器
		cout << "\n测试移进-归约分析器" << endl;

		try
		{
			// 创建分析器实例
			ShiftReduceParser Parser(SLRTable);

			// 根据当前选择的语法文件读取相应的词法分析器输出文件
			string TokenFile;
			if (GrammarFile == "Expr.grammar")
			{
				TokenFile = "ExprTokensOutput.txt";
			}
			else if (GrammarFile == "Tiny.grammar")
			{
				TokenFile = "TinyTokensOutput.txt";
			}
			else if (GrammarFile == "MiniC.grammar")
			{
				TokenFile = "MiniCTokensOutput.txt";
			}
			else
			{
				cout << "未知的语法文件" << endl;
				return 1;
			}

			// 从文件解析
			cout << "\n从文件中解析tokens: " << TokenFile << endl;

			bool Success = Parser.ParseFromFile(TokenFile);
			if (Success)
			{
				cout << "\n移进-归约分析成功！" << endl;
			}
			else
			{
				cout << "\n移进-归约分析失败！" << endl;
			}
		}
		catch (const exception &e)
		{
			cout << "移进-归约分析测试错误: " << e.what() << endl;
		}
	}
	catch (const exception &e)
	{
		cout << "错误: " << e.what() << endl;
		return 1;
	}

	return 0;
}