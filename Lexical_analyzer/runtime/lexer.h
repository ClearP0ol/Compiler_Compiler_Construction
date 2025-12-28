#pragma once

#include <string>
#include "token.h"
#include "dfa.h"

/*
 * Lexer
 * =====
 * 基于 DFA 的词法分析器（Longest Match）
 */
class Lexer {
public:
    // input: 源代码字符串
    // dfa:   已构造完成的 DFA
    Lexer(const std::string& input, DFA& dfa);

    // 获取下一个 Token
    Token nextToken();

private:
    const std::string& src;  // 输入源代码
    size_t pos = 0;          // 当前扫描位置（字节索引）

    int line = 1;            // 当前行号（从 1 开始）
    int column = 1;          // 当前列号（从 1 开始）

    DFA& dfa;                // DFA 引用

private:
    // 吃掉一个字符，并同步维护行列号
    void advance();

    // 跳过空白字符（space / tab / newline）
    void skipWhitespace();
};
