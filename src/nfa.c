#include "parserTypes.h"
#include <nfa.h>
#include <ccSet.h>
#include <stdint.h>
#include <stdlib.h>

int compareTransitions(transition_t* a, transition_t* b);

void buildNFA_Concatenation(ccSet_t* nfa, expressionConcatenation_t* expression){

}

/* NOTE: order does matter. We first must descend towards the leaves and after
 * those functions return we can modify the NFA with the associated operation */
void buildNFA_Alternation(ccSet_t* nfa, expressionAlternation_t* expression)
{
    switch(expression->type){
    /* if the  */
    case typeAlternation_AlternationConcatenation:
        buildNFA_Concatenation(nfa, expression->concatenation);
        /* do we do anything here ? */
        buildNFA_Alternation(nfa, expression->alternation);
        break;
    case typeAlternation_Concatenation:
        buildNFA_Concatenation(nfa, expression->concatenation);
        break;
    case typeAlternation_NULL:
    case typeAlternation_Empty:
    default:
        break;
    }
}

void buildNFA_RegularExpression(ccSet_t* nfa, expressionRegularExpression_t* expression)
{
    switch(expression->type){
    case typeRegularExpression_RegularExpression:
        buildNFA_Alternation(nfa, expression->alternation);
        break;
    case typeRegularExpression_NULL:
    case typeRegularExpression_Empty:
    case typeRegularExpression_EOF:
    default:
        break;        
    }
}

/* Ideally we should interleave the NFA building code with the actual 
 * parsing code. I don't see any reason to do it after parsing other 
 * than to delay a refactoring of the code */
void buildNFA(expressionRegularExpression_t* expression)
{
    ccSet_t* nfa = ccSet_ctor((void (*)(void *))transition_dtor, (int (*)(void *, void *))compareTransitions);
    buildNFA_RegularExpression(nfa, expression);

    return;
}

void transition_dtor(transition_t *data)
{
    free(data);
}

transition_t* transition_ctor(int fromState, int toState, bool isComplement, char symbol)
{
    transition_t* transition = malloc(sizeof(transition_t));
    transition->fromState = fromState;
    transition->toState = toState;
    transition->isComplement = isComplement;
    transition->symbol = symbol;

    return transition;
}

int compareTransitions(transition_t* a, transition_t* b)
{
    uint64_t A = *(uint64_t*)a;
    uint64_t B = *(uint64_t*)b;

    if(A > B)
        return 1;
    if(A < B)
        return -1;
    return 0;
}