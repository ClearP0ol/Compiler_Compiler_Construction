#include "lexer_rule_parser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

/*
 * tokenFromString
 * ===============
 * 将规则文件中的 token 名称映射为 TokenType
 */
TokenType LexerRuleParser::tokenFromString(const std::string& name) {
    if (name == "ID") return TokenType::ID;
    if (name == "NUM") return TokenType::NUM;

    if (name == "INT") return TokenType::INT;
    if (name == "VOID") return TokenType::VOID;
    if (name == "IF") return TokenType::IF;
    if (name == "ELSE") return TokenType::ELSE;
    if (name == "WHILE") return TokenType::WHILE;
    if (name == "RETURN") return TokenType::RETURN;

    if (name == "PLUS") return TokenType::PLUS;
    if (name == "MINUS") return TokenType::MINUS;
    if (name == "MULT") return TokenType::MULT;
    if (name == "DIV") return TokenType::DIV;

    if (name == "ASSIGN") return TokenType::ASSIGN;
    if (name == "EQ") return TokenType::EQ;
    if (name == "NEQ") return TokenType::NEQ;
    if (name == "LT") return TokenType::LT;
    if (name == "GT") return TokenType::GT;
    if (name == "LTE") return TokenType::LTE;
    if (name == "GTE") return TokenType::GTE;

    if (name == "LPAREN") return TokenType::LPAREN;
    if (name == "RPAREN") return TokenType::RPAREN;
    if (name == "LBRACE") return TokenType::LBRACE;
    if (name == "RBRACE") return TokenType::RBRACE;
    if (name == "SEMI") return TokenType::SEMI;
    if (name == "COMMA") return TokenType::COMMA;
    
    if (name == "READ")  return TokenType::READ;
    if (name == "WRITE") return TokenType::WRITE;


    throw std::runtime_error("Unknown token name: " + name);
}

/*
 * parseFromFile
 * =============
 * 解析规则文件
 */
RuleSet LexerRuleParser::parseFromFile(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open lexer rule file: " + filename);
    }

    RuleSet rules;
    std::string line;

    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string tokenName, pattern;
        iss >> tokenName >> pattern;

        if (tokenName.empty() || pattern.empty()) continue;

        LexerRule rule;
        rule.type = tokenFromString(tokenName);
        rule.pattern = pattern;

        rules.rules.push_back(rule);
    }

    return rules;
}
