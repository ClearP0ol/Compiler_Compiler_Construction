#pragma once
#include <string>
#include <vector>
#include "token.h"

/*
 * LexerRule
 * =========
 * 一条词法规则
 */
struct LexerRule {
    TokenType type;        // Token 类型
    std::string pattern;  // 词法模式（literal / {ID} / {NUM}）
};

/*
 * RuleSet
 * =======
 * 规则集合
 */
struct RuleSet {
    std::vector<LexerRule> rules;
};
