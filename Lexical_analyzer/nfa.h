#pragma once

#include <map>
#include <vector>
#include <set>
#include "token.h"

/*
 * State
 * =====
 * NFA 状态结点
 */
struct State {
    int id;   // 状态编号（仅用于调试 / 打印）

    // 字符转移：ch -> 多个目标状态
    std::map<char, std::vector<State*>> trans;

    // ε 转移
    std::vector<State*> eps;

    // 接受态对应的 Token
    // 非接受态为 ERROR
    TokenType acceptToken = TokenType::ERROR;
};

/*
 * NFA
 * ===
 * Thompson 构造的结果：一个开始状态 + 一个接受状态
 */
struct NFA {
    State* start;
    State* accept;
};

/*
 * newState
 * ========
 * 创建一个新的 NFA 状态
 * id 自动递增，便于调试
 */
State* newState();
