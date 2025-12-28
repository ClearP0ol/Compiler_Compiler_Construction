#include "lexer_generator.h"

#include <stdexcept>

#include "lexer_rule.h"
#include "lexer_rule_parser.h"
#include "thompson.h"
#include "dfa.h"
#include "dfa_min.h"

void LexerGenerator::loadRuleFile(const std::string& filename) {
    ruleFile = filename;
}

DFA LexerGenerator::buildDFA() {
    if (ruleFile.empty()) {
        throw std::runtime_error("Lexer rule file not set");
    }

    // 1. 解析 .lex 规则
    RuleSet rules = LexerRuleParser::parseFromFile(ruleFile);

    // 2. 规则 → NFA
    State* nfaStart = buildNFAFromRules(rules);

    // 3. NFA → DFA
    DFA dfa = ::buildDFA(nfaStart);

    // 4. DFA 最小化
    return minimizeDFA(dfa);
}
