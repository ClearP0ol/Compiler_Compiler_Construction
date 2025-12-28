#include "dfa_min.h"
#include <map>
#include <set>
#include <vector>

/*
 * 初始划分：
 *  - 非接受态
 *  - 接受态（按 TokenType 再细分）
 */
static std::vector<std::set<DFAState*>> initialPartition(const DFA& dfa) {
    std::map<TokenType, std::set<DFAState*>> acceptGroups;
    std::set<DFAState*> nonAccept;

    for (auto* s : dfa.states) {
        if (!s->isAccept) {
            nonAccept.insert(s);
        } else {
            acceptGroups[s->acceptToken].insert(s);
        }
    }

    std::vector<std::set<DFAState*>> P;
    if (!nonAccept.empty()) {
        P.push_back(nonAccept);
    }
    for (auto& [_, group] : acceptGroups) {
        P.push_back(group);
    }
    return P;
}

/*
 * 查找状态属于哪个分组
 */
static int findBlock(
    const std::vector<std::set<DFAState*>>& P,
    DFAState* s
) {
    for (int i = 0; i < (int)P.size(); ++i) {
        if (P[i].count(s)) return i;
    }
    return -1;
}

DFA minimizeDFA(const DFA& dfa) {
    auto P = initialPartition(dfa);
    bool changed = true;

    while (changed) {
        changed = false;
        std::vector<std::set<DFAState*>> newP;

        for (auto& block : P) {
            std::map<
                std::map<char, int>,
                std::set<DFAState*>
            > splitter;

            for (auto* s : block) {
                std::map<char, int> signature;

                for (auto& [ch, to] : s->trans) {
                    signature[ch] = findBlock(P, to);
                }

                splitter[signature].insert(s);
            }

            if (splitter.size() == 1) {
                newP.push_back(block);
            } else {
                changed = true;
                for (auto& [_, part] : splitter) {
                    newP.push_back(part);
                }
            }
        }
        P = std::move(newP);
    }

    // ===== 构造新 DFA =====
    DFA newDFA;
    std::map<DFAState*, DFAState*> rep;

    for (auto& block : P) {
        DFAState* s = new DFAState();
        s->id = newDFA.states.size();

        DFAState* any = *block.begin();
        s->isAccept = any->isAccept;
        s->acceptToken = any->acceptToken;

        newDFA.states.push_back(s);

        for (auto* old : block) {
            rep[old] = s;
        }
    }

    // 设置 start
    newDFA.start = rep[dfa.start];

    // 设置转移
    for (auto& block : P) {
        DFAState* from = rep[*block.begin()];
        DFAState* old = *block.begin();

        for (auto& [ch, to] : old->trans) {
            from->trans[ch] = rep[to];
        }
    }

    return newDFA;
}
