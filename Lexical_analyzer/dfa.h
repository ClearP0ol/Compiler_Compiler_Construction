#pragma once

#include <map>
#include <set>
#include <vector>
#include "token.h"
#include "nfa.h"

/*
 * DFAState
 * ========
 * DFA 中的一个状态
 * 本质：一组 NFA 状态的 ε-closure
 */
struct DFAState {
    int id;   // DFA 状态编号（调试用）

    // DFA 转移：字符 -> 唯一目标状态
    std::map<char, DFAState*> trans;

    // 是否为接受态
    bool isAccept = false;

    // 接受态对应的 Token
    TokenType acceptToken = TokenType::ERROR;

    // 该 DFA 状态对应的 NFA 状态集合
    std::set<State*> nfaStates;
};

/*
 * DFA
 * ===
 * 确定有限自动机
 */
struct DFA {
    DFAState* start;                 // 起始状态
    std::vector<DFAState*> states;   // 所有 DFA 状态（便于遍历 / 释放）
};

/*
 * buildDFA
 * ========
 * 子集构造法：
 * 从 NFA 起始状态构造 DFA
 */
DFA buildDFA(State* nfaStart);
