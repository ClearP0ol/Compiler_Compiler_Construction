#include "lexer.h"
#include "charset.h"

/*
 * 构造函数
 */
Lexer::Lexer(const std::string& input, DFA& dfa)
    : src(input), dfa(dfa) {}

/*
 * nextToken
 * =========
 * 从当前位置扫描下一个 Token（Longest Match）
 */
Token Lexer::nextToken() {
    // 1. 跳过空白字符
    skipWhitespace();

    // 2. 文件结束
    if (pos >= src.size()) {
        return {TokenType::ENDFILE, "", line, column};
    }

    // 记录 token 起始位置
    size_t startPos = pos;
    int startLine = line;
    int startColumn = column;

    DFAState* cur = dfa.start;

    // 记录最近一次接受态（Longest Match）
    DFAState* lastAccept = nullptr;
    size_t lastAcceptPos = pos;

    // 用 i 在 DFA 上“试跑”，不真正吃字符
    size_t i = pos;

    // 3. DFA 试跑
    while (i < src.size()) {
        char ch = src[i];

        auto it = cur->trans.find(ch);
        if (it == cur->trans.end()) {
            break;
        }

        cur = it->second;
        i++;

        if (cur->isAccept) {
            lastAccept = cur;
            lastAcceptPos = i;
        }
    }

    // 4. 成功匹配（Longest Match）
    if (lastAccept) {
        // 真正推进输入指针（只能用 advance）
        while (pos < lastAcceptPos) {
            advance();
        }

        return {
            lastAccept->acceptToken,
            src.substr(startPos, lastAcceptPos - startPos),
            startLine,
            startColumn
        };
    }

    // 5. 词法错误：非法字符
    char badChar = src[pos];
    advance();  // 吃掉非法字符，防止死循环

    return {
        TokenType::ERROR,
        std::string(1, badChar),
        startLine,
        startColumn
    };
}

/*
 * advance
 * =======
 * 吃掉一个字符，并维护行列号
 */
void Lexer::advance() {
    char c = src[pos++];

    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
}

/*
 * skipWhitespace
 * ==============
 * 跳过空白字符（空格 / 制表 / 换行）
 */
void Lexer::skipWhitespace() {
    while (pos < src.size() && isWhitespace(src[pos])) {
        advance();
    }
}
