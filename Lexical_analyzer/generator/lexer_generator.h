#pragma once

#include <string>
#include "dfa.h"

/*
 * LexerGenerator
 * ==============
 * 词法分析器生成器（Compiler-Compiler 的 lexer 部分）
 *
 * 职责：
 * - 读取 .lex 规则文件
 * - 基于规则构造 DFA
 */
class LexerGenerator {
public:
    // 读取规则文件
    void loadRuleFile(const std::string& filename);

    // 构造 DFA（正则 → NFA → DFA → 最小化）
    DFA buildDFA();

private:
    std::string ruleFile;
};
