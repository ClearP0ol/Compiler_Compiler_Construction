#include "FirstFollowCalculator.hpp"
#include "LRAutomaton.hpp"
#include <iostream>

using namespace std;

int main() {
    // 创建语法加载器
    GrammarLoader Loader;

    // 测试文件路径
    string GrammarFile = "MiniC.grammar";

    // 加载语法
    GrammarDefinition Grammar = Loader.LoadFromFile(GrammarFile);

    // 检查是否加载成功
    if (Grammar.Productions.empty()) {
        cout << endl << "语法加载失败！" << endl;
        return 1;
    }

    cout << endl << "语法加载成功！" << endl;

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

    try {
        // 创建LR(0)自动机
        LRAutomatonBuilder AutomatonBuilder(Grammar);

        // 打印自动机信息
        cout << "LR(0)自动机构建成功！" << endl;
        cout << "自动机状态数量: " << AutomatonBuilder.States.size() << endl;

        // 打印增广语法信息
        cout << "增广语法开始符号: " << Grammar.StartSymbol.Name << "'" << endl;

        // 打印所有状态
        AutomatonBuilder.PrintAutomaton();

    }
    catch (const exception& e) {
        cout << "LR(0)自动机构建失败: " << e.what() << endl;
        return 1;
    }

    return 0;
}