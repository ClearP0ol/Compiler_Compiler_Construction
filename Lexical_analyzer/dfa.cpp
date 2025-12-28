#include "dfa.h"
#include <queue>
#include <map>
#include <algorithm>

/*
 * ε-closure
 * =========
 * 计算一组 NFA 状态的 ε 闭包
 */
static std::set<State*> epsilonClosure(const std::set<State*>& states) {
    std::set<State*> closure = states;
    std::queue<State*> q;

    for (auto* s : states)
        q.push(s);

    while (!q.empty()) {
        State* s = q.front();
        q.pop();

        for (auto* t : s->eps) {
            if (!closure.count(t)) {
                closure.insert(t);
                q.push(t);
            }
        }
    }
    return closure;
}

/*
 * move
 * ====
 * 从一组 NFA 状态，经字符 ch 能到达的状态集合
 */
static std::set<State*> moveSet(const std::set<State*>& states, char ch) {
    std::set<State*> result;
    for (auto* s : states) {
        auto it = s->trans.find(ch);
        if (it != s->trans.end()) {
            for (auto* t : it->second)
                result.insert(t);
        }
    }
    return result;
}

/*
 * 从 NFA 状态集合中选取 Token
 * ==============================
 * 规则：
 * 1. 若集合中存在接受态
 * 2. 选 TokenType != ERROR
 * 3. 若多个，按枚举顺序（关键字优先）
 */
static void chooseAcceptToken(DFAState* dfaState) {
    TokenType best = TokenType::ERROR;

    for (auto* s : dfaState->nfaStates) {
        if (s->acceptToken != TokenType::ERROR) {
            if (best == TokenType::ERROR ||
                tokenPriority(s->acceptToken) <
                tokenPriority(best)) {
                best = s->acceptToken;
            }
        }
    }

    if (best != TokenType::ERROR) {
        dfaState->isAccept = true;
        dfaState->acceptToken = best;
    }
}

/*
 * buildDFA
 * ========
 * 子集构造主算法
 */
DFA buildDFA(State* nfaStart) {
    DFA dfa;
    int dfaId = 0;

    // 起始 ε-closure
    std::set<State*> startSet;
    startSet.insert(nfaStart);
    startSet = epsilonClosure(startSet);

    auto* startDFA = new DFAState();
    startDFA->id = dfaId++;
    startDFA->nfaStates = startSet;
    chooseAcceptToken(startDFA);

    dfa.start = startDFA;
    dfa.states.push_back(startDFA);

    // 用 map 判重：NFA 状态集合 -> DFAState*
    std::map<std::set<State*>, DFAState*> dfaMap;
    dfaMap[startSet] = startDFA;

    std::queue<DFAState*> worklist;
    worklist.push(startDFA);

    while (!worklist.empty()) {
        DFAState* cur = worklist.front();
        worklist.pop();

        // 收集所有可能的输入字符
        std::set<char> alphabet;
        for (auto* s : cur->nfaStates) {
            for (auto& [ch, _] : s->trans)
                alphabet.insert(ch);
        }

        // 对每个字符做 move + ε-closure
        for (char ch : alphabet) {
            std::set<State*> moved = moveSet(cur->nfaStates, ch);
            if (moved.empty()) continue;

            std::set<State*> nextSet = epsilonClosure(moved);

            DFAState* nextDFA = nullptr;
            auto it = dfaMap.find(nextSet);
            if (it != dfaMap.end()) {
                nextDFA = it->second;
            } else {
                nextDFA = new DFAState();
                nextDFA->id = dfaId++;
                nextDFA->nfaStates = nextSet;
                chooseAcceptToken(nextDFA);

                dfa.states.push_back(nextDFA);
                dfaMap[nextSet] = nextDFA;
                worklist.push(nextDFA);
            }

            cur->trans[ch] = nextDFA;
        }
    }

    return dfa;
}
