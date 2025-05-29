#pragma once

#include <ccList.h>
#include <parserTypes.h>

typedef struct transition{
    int fromState;
    int toState;
    char isComplement;
    char isEpsilon;
    char symbol;
}transition_t;

typedef struct nfa{
    ccListNode_t* start;
    ccListNode_t* accept;
    ccList_t* states;
}nfa_t;

transition_t* transition_ctor(int fromState, int toState, char isComplement, char isEpsilon, char symbol);
void transition_dtor(transition_t* data);
nfa_t* nfa_ctor(void);
void nfa_dtor(nfa_t* data);

nfa_t* buildNFA(expressionRegularExpression_t* expression);

void dbg_printNFA(nfa_t* nfa);