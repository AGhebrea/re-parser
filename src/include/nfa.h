#pragma once

#include<parserTypes.h>

typedef struct transition{
    short fromState;
    short toState;
    short isComplement;
    char symbol;
}transition_t;

transition_t* transition_ctor(int fromState, int toState, bool isComplement, char symbol);
void transition_dtor(transition_t*);

void buildNFA(expressionRegularExpression_t* expression);