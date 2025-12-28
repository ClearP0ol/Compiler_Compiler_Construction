#pragma once

/*
 * 字符分类工具
 * ============
 * Lexer / Regex / Automata 公用
 *
 * 注意：
 * - 不使用 <cctype>，避免 locale 和 UB
 * - 统一使用 unsigned char，保证安全
 */

constexpr bool isLetter(unsigned char c) noexcept {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

constexpr bool isDigit(unsigned char c) noexcept {
    return (c >= '0' && c <= '9');
}

constexpr bool isUnderscore(unsigned char c) noexcept {
    return c == '_';
}

// 标识符字符：[A-Za-z0-9_]
constexpr bool isIdentChar(unsigned char c) noexcept {
    return isLetter(c) || isDigit(c) || isUnderscore(c);
}

// 空白字符（不含换行）
// 用于：跳过 token 间隔
constexpr bool isBlank(unsigned char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r';
}

// 换行符（单独判断）
// 用于：行号统计
constexpr bool isNewline(unsigned char c) noexcept {
    return c == '\n';
}
