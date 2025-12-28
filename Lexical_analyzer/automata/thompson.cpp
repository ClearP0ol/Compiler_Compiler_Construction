#include "thompson.h"
#include <stdexcept>

/*
 * Thompson 构造法
 * ===============
 * Regex AST -> NFA
 */
NFA buildNFA(RegexNode* node) {
    if (!node) {
        throw std::runtime_error("Null regex node");
    }

    switch (node->type) {
    case RegexType::CHAR: {
        State* s = newState();
        State* t = newState();
        s->trans[node->ch].push_back(t);
        return {s, t};
    }

    case RegexType::CONCAT: {
        NFA a = buildNFA(node->left);
        NFA b = buildNFA(node->right);
        a.accept->eps.push_back(b.start);
        return {a.start, b.accept};
    }

    case RegexType::UNION: {
        State* s = newState();
        State* t = newState();
        NFA a = buildNFA(node->left);
        NFA b = buildNFA(node->right);

        s->eps.push_back(a.start);
        s->eps.push_back(b.start);
        a.accept->eps.push_back(t);
        b.accept->eps.push_back(t);

        return {s, t};
    }

    case RegexType::STAR: {
        State* s = newState();
        State* t = newState();
        NFA a = buildNFA(node->left);

        s->eps.push_back(a.start);
        s->eps.push_back(t);
        a.accept->eps.push_back(a.start);
        a.accept->eps.push_back(t);

        return {s, t};
    }

    default:
        throw std::runtime_error("Unknown regex node type");
    }
}


RegexNode* buildKeyword(const std::string& s) {
    if (s.empty()) return nullptr;

    RegexNode* node = new RegexNode(RegexType::CHAR, s[0]);
    for (size_t i = 1; i < s.size(); ++i) {
        node = new RegexNode(
            RegexType::CONCAT, 0,
            node,
            new RegexNode(RegexType::CHAR, s[i])
        );
    }
    return node;
}

RegexNode* buildCharSet(const std::vector<char>& chars) {
    if (chars.empty()) return nullptr;

    RegexNode* node = new RegexNode(RegexType::CHAR, chars[0]);
    for (size_t i = 1; i < chars.size(); ++i) {
        node = new RegexNode(
            RegexType::UNION, 0,
            node,
            new RegexNode(RegexType::CHAR, chars[i])
        );
    }
    return node;
}

RegexNode* buildIDRegex() {
    std::vector<char> head;
    for (char c = 'a'; c <= 'z'; ++c) head.push_back(c);
    for (char c = 'A'; c <= 'Z'; ++c) head.push_back(c);
    head.push_back('_');

    std::vector<char> tail = head;
    for (char c = '0'; c <= '9'; ++c) tail.push_back(c);

    RegexNode* headNode = buildCharSet(head);
    RegexNode* tailNode = new RegexNode(
        RegexType::STAR, 0,
        buildCharSet(tail)
    );

    return new RegexNode(
        RegexType::CONCAT, 0,
        headNode,
        tailNode
    );
}

RegexNode* buildNUMRegex() {
    std::vector<char> digits;
    for (char c = '0'; c <= '9'; ++c) digits.push_back(c);

    RegexNode* digit = buildCharSet(digits);

    // [0-9]+  =>  [0-9][0-9]*
    return new RegexNode(
        RegexType::CONCAT, 0,
        digit,
        new RegexNode(RegexType::STAR, 0, digit)
    );
}

State* buildMasterNFA(
    const std::vector<std::pair<TokenType, RegexNode*>>& specs
) {
    State* start = newState();

    for (auto& [tok, regex] : specs) {
        NFA nfa = buildNFA(regex);
        nfa.accept->acceptToken = tok;
        start->eps.push_back(nfa.start);
    }
    return start;
}


/*
 * buildNFAFromRules
 * =================
 * 根据 RuleSet（来自 .lex）构造总 NFA
 */
State* buildNFAFromRules(const RuleSet& rules) {
    std::vector<std::pair<TokenType, RegexNode*>> specs;

    for (const auto& rule : rules.rules) {
        RegexNode* regex = nullptr;

        // ===== 特殊模式 =====
        if (rule.pattern == "{ID}") {
            regex = buildIDRegex();
        }
        else if (rule.pattern == "{NUM}") {
            regex = buildNUMRegex();
        }
        // ===== 关键字或字面量 =====
        else {
            // 对于 "if" "+" "==" 等
            regex = buildKeyword(rule.pattern);
        }

        specs.push_back({rule.type, regex});
    }

    return buildMasterNFA(specs);
}
