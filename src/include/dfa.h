#pragma once

#include "./nfa.h"
#include "ccDynamicArray.h"
#include "ccList.h"
#include "ccRBTree.h"
#include "ccRuntime.h"

typedef struct dfa_state{
    ccType_t type;
    stateType_t state;
    ccRBTree_t* set;
}dfa_state_t;

typedef struct dfa_transition{
    ccType_t type;
    dfa_state_t* fromState;
    dfa_state_t* toState;
    char isComplement;
    char symbol;
}dfa_transition_t;

typedef struct dfa{
    stateType_t start;
    ccRBTree_t* acceptStates;
    ccDynamicArray_t* transitions;
    ccDynamicArray_t* reverseTransitions;
    ccList_t* states;
    /* used for freeing memory */
    ccList_t* setList;
    ccRBTree_t* stateSet;
    dfa_state_t* startState;
}dfa_t;

dfa_t* buildDFA(nfa_t* nfa);
dfa_t* minimizeDFA(dfa_t* dfa);

dfa_t* dfa_ctor();
void dfa_dtor(dfa_t* data);

dfa_state_t* dfa_state_ctor(stateType_t state, ccRBTree_t* set);
void dfa_state_dtor(dfa_state_t* data);

dfa_transition_t* dfa_transition_ctor(dfa_state_t* fromState, dfa_state_t* toState, char isComplement, char symbol);
void dfa_transition_dtor(dfa_transition_t* data);

void dbg_printDFA(dfa_t* dfa);