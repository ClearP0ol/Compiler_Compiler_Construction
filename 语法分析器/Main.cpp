// test_grammar_loader.cpp
#include "GrammarLoader.hpp"
#include <iostream>

using namespace std;

int main() {

    // 创建文法加载器
    GrammarLoader Loader;

    // 测试文件路径
    string GrammarFile = "MiniC.grammar";

    // 加载文法
    cout << "正在加载文法文件: " << GrammarFile << endl;
    GrammarDefinition Grammar = Loader.LoadFromFile(GrammarFile);

    // 检查是否加载成功
    if (Grammar.Productions.empty()) {
        cout << endl << "文法加载失败！" << endl;
        return 1;
    }

    cout << endl << "文法加载成功！" << endl;

   
    return 0;
}