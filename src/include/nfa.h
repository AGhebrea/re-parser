#pragma once

#include <ccList.h>
#include <ccDynamicArray.h>
#include <parserTypes.h>

typedef size_t stateType_t;

typedef struct transition{
    stateType_t fromState;
    stateType_t toState;
    char isComplement;
    char isEpsilon;
    char symbol;
}transition_t;

typedef struct nfa{
    ccListNode_t* start;
    ccListNode_t* accept;
    ccList_t* transitions;
    ccDynamicArray_t* states;
}nfa_t;

transition_t* transition_ctor(stateType_t fromState, stateType_t toState, char isComplement, char isEpsilon, char symbol);
void transition_dtor(transition_t* data);
nfa_t* nfa_ctor(void);
void nfa_dtor(nfa_t* data);

nfa_t* buildNFA(expressionRegularExpression_t* expression);

void dbg_printNFA(nfa_t* nfa);