#include "FirstFollowCalculator.hpp"
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

    return 0;
}