#pragma once

/*
 * RegexType
 * =========
 * Thompson 构造法所需的最小正则算子集合
 */
enum class RegexType {
    CHAR,     // 单字符 a
    CONCAT,   // 连接 ab
    UNION,    // 或 a|b
    STAR      // 闭包 a*
};

/*
 * RegexNode
 * =========
 * 正则表达式抽象语法树
 */
struct RegexNode {
    RegexType type;

    // 仅 CHAR 使用
    char ch = 0;

    // 子结点
    RegexNode* left = nullptr;
    RegexNode* right = nullptr;

    RegexNode(RegexType t,
              char c = 0,
              RegexNode* l = nullptr,
              RegexNode* r = nullptr)
        : type(t), ch(c), left(l), right(r) {}
};
