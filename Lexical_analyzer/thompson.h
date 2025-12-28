#pragma once

#include "regex_ast.h"
#include "nfa.h"
#include "token.h"

/*
 * buildNFA
 * ========
 * Thompson 构造法核心：
 * 将正则 AST 转换为 NFA
 */
NFA buildNFA(RegexNode* node);

/*
 * ===== 下面是“正则构造工具函数” =====
 * 它们不属于 Thompson 算法本身
 * 但用于方便构造 Token 对应的正则
 */

// 关键字：如 "int" / "while"
RegexNode* buildKeyword(const std::string& s);

// 字符集合：[a-zA-Z_] / [0-9]
RegexNode* buildCharSet(const std::vector<char>& chars);

// 标识符：ID = [a-zA-Z_][a-zA-Z0-9_]*
RegexNode* buildIDRegex();

// 整数常量：NUM = [0-9]+
RegexNode* buildNUMRegex();

State* buildLexerNFA();


/*
 * buildMasterNFA
 * ==============
 * 将多个 token 的 NFA 合并为一个总入口
 */
State* buildMasterNFA(
    const std::vector<std::pair<TokenType, RegexNode*>>& specs
);
