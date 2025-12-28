#pragma once

/*
 * 字符分类工具
 * ============
 * Lexer / Regex 构造公用
 */

// 是否为字母
inline bool isLetter(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

// 是否为数字
inline bool isDigit(char c) {
    return (c >= '0' && c <= '9');
}

// 是否为下划线
inline bool isUnderscore(char c) {
    return c == '_';
}

// 是否为标识符字符
inline bool isIdentChar(char c) {
    return isLetter(c) || isDigit(c) || isUnderscore(c);
}

// 是否为空白字符（Lexer 用）
inline bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
