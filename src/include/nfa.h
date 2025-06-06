#pragma once

#include <ccList.h>
#include <ccDynamicArray.h>
#include <parserTypes.h>

typedef size_t stateType_t;

typedef struct nfa_transition{
    stateType_t fromState;
    stateType_t toState;
    char isComplement;
    char isEpsilon;
    char symbol;
}nfa_transition_t;

typedef struct nfa{
    ccListNode_t* start;
    ccListNode_t* accept;
    ccList_t* transitions;
    ccDynamicArray_t* states;
}nfa_t;

nfa_transition_t* transition_ctor(stateType_t fromState, stateType_t toState, char isComplement, char isEpsilon, char symbol);
void transition_dtor(nfa_transition_t* data);
nfa_t* nfa_ctor(void);
void nfa_dtor(nfa_t* data);

nfa_t* buildNFA(expressionRegularExpression_t* expression);

void dbg_printNFA(nfa_t* nfa);