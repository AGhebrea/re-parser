#pragma once

#include "./nfa.h"
#include "ccDynamicArray.h"
#include "ccList.h"
#include "ccRBTree.h"

typedef struct dfa_state{
    stateType_t state;
    ccRBTree_t* set;
}dfa_state_t;

typedef struct dfa_transition{
    dfa_state_t* fromState;
    dfa_state_t* toState;
    char isComplement;
    // todo: remove isepsilon as the whole point of what we are doing is to remove them from graph
    char isEpsilon;
    char symbol;
}dfa_transition_t;

/* We need to store:
 * - transitions
 * - states
 * - start state
 * - accept state list
 * We must be able to:
 * - iterate over states, a ll would be ok
 * - find a transition for a particular state, that means
 * that we need to do a fixup again and store the states like in the nfa.
 * - for the set minimization we theoretically have to be able
 * to find transitions by chars. this means that we will store 
 * an aux DS, that is a RBTree which has a key a char, and returns a ll of
 * all transitions containing that char. it might be a really bombastic thing 
 * to do but idk. btw the rbtree has to store copies of the ll nodes */
typedef struct dfa{
    stateType_t start;
    ccRBTree_t* acceptStates;
    ccDynamicArray_t* states;
    /* really bombastic stuff */
    ccRBTree_t* auxTransitionsOnChar;
    /* used for freeing memory */
    ccList_t* setList;
    /* used for freeing memory */
    dfa_state_t* startState;
}dfa_t;

dfa_t* buildDFA(nfa_t* nfa);
dfa_t* dfa_ctor();
void dfa_dtor(dfa_t* data);

dfa_state_t* dfa_state_ctor(stateType_t state, ccRBTree_t* set);
void dfa_state_dtor(dfa_state_t* data);

dfa_transition_t* dfa_transition_ctor(dfa_state_t* fromState, dfa_state_t* toState, char isComplement, char isEpsilon, char symbol);
void dfa_transition_dtor(dfa_transition_t* data);