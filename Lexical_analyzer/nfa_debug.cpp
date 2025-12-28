#include "nfa.h"
#include <iostream>
#include <set>

/*
 * dfsPrint
 * ========
 * 深度优先遍历 NFA 并打印所有边
 */
static void dfsPrint(State* s, std::set<State*>& visited) {
    if (!s || visited.count(s)) return;
    visited.insert(s);

    // 普通字符转移
    for (auto& [ch, targets] : s->trans) {
        for (auto* t : targets) {
            std::cout << "S" << s->id
                      << " --" << ch << "--> "
                      << "S" << t->id << "\n";
        }
    }

    // ε 转移
    for (auto* t : s->eps) {
        std::cout << "S" << s->id
                  << " --ε--> "
                  << "S" << t->id << "\n";
    }

    // 继续 DFS
    for (auto& [_, targets] : s->trans)
        for (auto* t : targets)
            dfsPrint(t, visited);

    for (auto* t : s->eps)
        dfsPrint(t, visited);
}

/*
 * printNFA
 * ========
 * 从 start 状态打印整个 NFA
 */
void printNFA(State* start) {
    std::set<State*> visited;
    dfsPrint(start, visited);
}
