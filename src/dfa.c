#include "./include/dfa.h"
#include "./include/nfa.h"
#include "ccDynamicArray.h"
#include "ccList.h"
#include "cclog.h"
#include <ccRBTree.h>
#include <ccStack.h>
#include <cclog_macros.h>
#include <stdlib.h>
#include <string.h>

ccRBTree_t* followEpsilon(nfa_t* nfa, stateType_t* stateIndex, bool* hasAcceptState);
int compareStates(void* a, void* b);
int compareByChar(void* a, void* b);

static void dbg_printTransition(dfa_transition_t* transition)
{
    ccLogDebug(
        "\n\tfromState: \t%ld\n"
        "\ttoState: \t%ld\n"
        "\tisCompl: \t%s\n"
        "\tisEpsil: \t%s\n"
        "\tsymbol: \t%c (0x%x)\n",
        transition->fromState->state,
        transition->toState->state,
        (int)transition->isComplement ? "True" : "False",
        (int)transition->isEpsilon ? "True" : "False",
        transition->symbol,
        transition->symbol
    );
}

void dbg_printTransitionList(ccList_t* list)
{
    dfa_transition_t* transition = NULL;

    ccLogDebug("LINKAGE START");
    for(size_t j = 0; j < list->size; ++j){
        transition = (dfa_transition_t*)ccList_itemAt(list, j);
        dbg_printTransition(transition);
    }
    ccLogDebug("LINKAGE END");
}

void dbg_printStates(dfa_t* dfa)
{
    ccList_t* list = NULL;

    for(size_t i = 0; i < dfa->states->size; ++i){
        list = *(ccList_t**)ccDynamicArray_get(dfa->states, i);
        if(list == NULL)
            continue;
        dbg_printTransitionList(list);
    }
}

void dbg_printSetData(void* data)
{
    int state = *(int*)data;

    ccLogDebug("%d ", state);
}

int compareNFATransitions(void* a, void* b)
{
    return memcmp(a, b, sizeof(nfa_transition_t));
}

static inline int hasDescendants(ccRBTreeNode_t* a)
{
    if(a->left == NULL && a->right == NULL)
        return 0;
    return 1;
}

int compareSetsImpl(ccRBTreeNode_t* a, ccRBTreeNode_t* b, int(*compare_fn)(void*, void*))
{
    int status = 0;

    if(a == NULL && b == NULL)
        return 0;
    if(a != NULL && b == NULL)
        return 1;
    if(b != NULL && a == NULL)
        return -1;

    if(!hasDescendants(a) && !hasDescendants(b))
        return compare_fn(a->item, b->item);

    status = compareSetsImpl(a->left, b->left, compare_fn);
    if(status == 0)
        status = compareSetsImpl(a->right, b->right, compare_fn);

    return status;    
}

int compareSets(void* a, void* b)
{
    ccRBTree_t* A = ((dfa_state_t*)a)->set;
    ccRBTree_t* B = ((dfa_state_t*)b)->set;

    if(A == B)
        return 0;

    return compareSetsImpl(A->head, B->head, compareSets);
}

void dbg_printStateSet(void* data)
{
    ccRBTree_t* set = (ccRBTree_t*)((dfa_state_t*)data)->set;
    stateType_t stateId = (stateType_t)((dfa_state_t*)data)->state;

    ccLogDebug("Printing Set with ID %ld:", stateId);
    dbg_printSet(set, dbg_printSetData);
}

void dbg_printAuxCharSet(void* data)
{
    ccRBTreeNode_t* node = (ccRBTreeNode_t*)data;
    ccLogDebug("START Printing tree node");
    ccLogDebug("key %c", *(char*)node->key);
    dbg_printTransitionList((node->item));
    ccLogDebug("END Printing tree node");
}

/* TODO: fix memleaks */
dfa_t* buildDFA(nfa_t* nfa)
{
    bool hasAcceptState = false;
    stateType_t stateId = 0;
    ccStack_t* workStack = ccStack_ctor(256, NULL);
    ccStack_t* walkStack = ccStack_ctor(256, NULL);
    ccRBTree_t* stateSet = ccRBTree_ctor(0, free, compareSets);
    dfa_state_t* fromState = NULL;
    dfa_state_t* toState = NULL;
    dfa_state_t* auxState = NULL;
    ccRBTree_t* fromSet = NULL;
    ccRBTree_t* toSet = NULL;
    ccList_t* list = NULL;
    ccListNode_t* node = NULL;
    ccRBTreeNode_t* setNode = NULL;
    ccRBTreeNode_t* auxSetNode = NULL;
    nfa_transition_t* nfa_transition = NULL;
    dfa_transition_t* dfa_transition = NULL;

    dfa_t* dfa = dfa_ctor();

    fromSet = followEpsilon(nfa, &((nfa_transition_t*)nfa->start->data)->fromState, &hasAcceptState);
    fromState = dfa_state_ctor(stateId, fromSet);
    dfa->start = fromState->state;
    stateId += 1;
    ccRBTree_insert(stateSet, ccRBTreeNode_ctor(fromState, NULL));
    ccStack_push(workStack, fromState);
    while(workStack->size != 0){
        fromState = ccStack_pop(workStack);
        ccStack_push(walkStack, fromState->set->head);
        while(walkStack->size != 0){
            hasAcceptState = false;
            setNode = ccStack_pop(walkStack);
            if(setNode->left != NULL){
                ccStack_push(walkStack, setNode->left);
            }
            if(setNode->right != NULL){
                ccStack_push(walkStack, setNode->right);
            }
            node = *(ccListNode_t**)ccDynamicArray_get(nfa->states, *(size_t*)(setNode->item));
            if(node != NULL){
                nfa_transition = (nfa_transition_t*)node->data;
                if(!nfa_transition->isEpsilon){
                    toSet = followEpsilon(nfa, &nfa_transition->toState, &hasAcceptState);
                    auxState = dfa_state_ctor(stateId, toSet);
                    auxSetNode = ccRBTree_find(stateSet, auxState);
                    if(auxSetNode == NULL){
                        toState = dfa_state_ctor(stateId, toSet);
                        stateId += 1;
                        ccRBTree_insert(stateSet, ccRBTreeNode_ctor(toState, NULL));
                        ccStack_push(workStack, toState);
                    }else{
                        toState = (dfa_state_t*)(auxSetNode->item);
                    }
                    dfa_transition = dfa_transition_ctor(fromState, toState, nfa_transition->isComplement, nfa_transition->isEpsilon, nfa_transition->symbol);
                    /* insert transition into ccDynamicArray_t* states; */
                    list = *(ccList_t**)ccDynamicArray_get(dfa->states, dfa_transition->fromState->state);
                    if(list == NULL){
                        list = ccList_ctor();
                        ccDynamicArray_set(dfa->states, dfa_transition->fromState->state, &list);
                    }
                    ccList_append(list, ccListNode_ctor(dfa_transition, dfa_transition_dtor));

                    // insert transition into ccRBTree_t* auxTransitionsOnChar;
                    auxSetNode = ccRBTree_find(dfa->auxTransitionsOnChar, &dfa_transition->symbol);
                    if(auxSetNode != NULL){
                        list = (ccList_t*)(auxSetNode->item);
                    }else{
                        list = ccList_ctor();
                        ccRBTree_insert(dfa->auxTransitionsOnChar, ccRBTreeNode_ctor(list, &dfa_transition->symbol)); 
                    }
                    ccList_append(list, ccListNode_ctor(dfa_transition, dfa_transition_dtor));

                    // if transition contains acceptState of nfa, add it to ccList_t* acceptStates;
                    if(hasAcceptState){
                        auxSetNode = ccRBTree_find(dfa->acceptStates, &dfa_transition->toState->state);
                        if(auxSetNode == NULL)
                            ccRBTree_insert(dfa->acceptStates, ccRBTreeNode_ctor(&dfa_transition->toState->state, NULL));
                    }
                }
            }
        }
    }

    dbg_printStates(dfa);
    dbg_printSet(dfa->auxTransitionsOnChar, dbg_printAuxCharSet);
    dbg_printSet(dfa->acceptStates, dbg_printSetData);

    ccStack_dtor(workStack);
    ccStack_dtor(walkStack);

    return NULL;
}

dfa_t* dfa_ctor()
{
    dfa_t* dfa;
    expectExit(dfa, malloc(sizeof(dfa_t)), != NULL);

    dfa->start = 0;
    dfa->acceptStates = ccRBTree_ctor(0, free, compareStates);
    dfa->states = ccDynamicArray_ctor(sizeof(ccList_t*), true);
    dfa->auxTransitionsOnChar = ccRBTree_ctor(1, dfa_transition_dtor, compareByChar);

    return dfa;
}

void dfa_dtor(dfa_t* dfa)
{
    ccLogNotImplemented;
}

/* TODO: could be just return (int)(a-b); */
int compareStates(void* a, void* b)
{
    stateType_t A, B;
    
    A = *(stateType_t*)a;
    B = *(stateType_t*)b;

    if(A > B)
        return 1;
    if(B > A)
        return -1;
    return 0;
}

int compareByChar(void* a, void* b)
{
    char A = *(char*)a;
    char B = *(char*)b;

    return A - B;
}

static inline int doFollow(ccRBTree_t* set, nfa_transition_t* transition)
{
    if(transition->isEpsilon && ccRBTree_find(set, &transition->toState) == NULL)
        return 1;
    return 0;
}

/* TODO: If you verify that there is no possibility for a epsilon cycle forming in the NFAs, you can 
 * remove the check that looks into the set to see if the node has already been visited.
 * There's also the possibility of building the whole list at once and referencing it. That might solve
 * the cycle check issue */
ccRBTree_t* followEpsilon(nfa_t* nfa, stateType_t* stateIndex, bool* hasAcceptState)
{
    ccListNode_t* state = NULL;
    ccListNode_t* nextState = NULL;
    ccStack_t* stack = ccStack_ctor(256, NULL);
    ccRBTree_t* set = ccRBTree_ctor(0, free, compareStates);
    
    ccRBTree_insert(set, ccRBTreeNode_ctor(stateIndex, NULL));
    state = *(ccListNode_t**)ccDynamicArray_get(nfa->states, *stateIndex);
    ccStack_push(stack, state);
    while(stack->size != 0){
        state = ccStack_pop(stack);
        while(state != NULL){
            if(doFollow(set, (nfa_transition_t*)state->data)){
                nextState = *(ccListNode_t**)ccDynamicArray_get(nfa->states, ((nfa_transition_t*)state->data)->toState);
                if(nextState != NULL){
                    ccStack_push(stack, nextState);
                }
                ccRBTree_insert(set, ccRBTreeNode_ctor(&((nfa_transition_t*)state->data)->toState, NULL));
                if(((nfa_transition_t*)state->data)->toState == ((nfa_transition_t*)(nfa->accept->data))->toState)
                    *hasAcceptState = true;
            }
            state = state->next;
        }
    }

    ccStack_dtor(stack);

    return set;
}

dfa_state_t* dfa_state_ctor(stateType_t state, ccRBTree_t* set)
{
    dfa_state_t* newState;
    expectExit(newState, malloc(sizeof(dfa_state_t)), != NULL);
    newState->state = state;
    newState->set = set;

    return newState;
}

void dfa_state_dtor(void* data)
{
    free(data);
}

dfa_transition_t* dfa_transition_ctor(dfa_state_t* fromState, dfa_state_t* toState, char isComplement, char isEpsilon, char symbol)
{
    dfa_transition_t* transition;
    
    expectExit(transition, malloc(sizeof(dfa_transition_t)), != NULL);
    transition->fromState = fromState;
    transition->toState = toState;
    transition->isComplement = isComplement;
    transition->isEpsilon = isEpsilon;
    transition->symbol = symbol;

    return transition;
}

void dfa_transition_dtor(void* data)
{
    ccLogNotImplemented;
}