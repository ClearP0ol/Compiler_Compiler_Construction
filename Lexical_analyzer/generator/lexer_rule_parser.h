#pragma once

#include <string>
#include "lexer_rule.h"

/*
 * LexerRuleParser
 * ===============
 * 解析 .lex 文件，生成 RuleSet
 */
class LexerRuleParser {
public:
    // 从规则文件读取 RuleSet
    static RuleSet parseFromFile(const std::string& filename);

private:
    // token 名称 → TokenType
    static TokenType tokenFromString(const std::string& name);
};
