#include "nfa.h"

/*
 * newState
 * ========
 * 创建一个新的 NFA 状态
 */
static int STATE_ID = 0;

State* newState() {
    State* s = new State();
    s->id = STATE_ID++;
    return s;
}
