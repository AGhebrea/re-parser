#include "./include/dfa.h"
#include "./include/nfa.h"
#include "ccDynamicArray.h"
#include "ccList.h"
#include "ccRuntime.h"
#include "cclog.h"
#include "ccstd.h"
#include <ccRBTree.h>
#include <ccStack.h>
#include <cclog_macros.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* TODO: remove unused stuff from DFA module */
/* TODO: see about optimizing minimization implementation because we've written too much bombastic code.
 * we can try seeing if we can use the transitionsOnChar set to optimize the DFA generation */

ccRBTree_t* followEpsilon(nfa_t* nfa, stateType_t* stateIndex, bool* hasAcceptState);

static inline int doFollow(ccRBTree_t* set, nfa_transition_t* transition);
static inline int hasDescendants(ccRBTree_t* set, ccRBTreeNode_t* a);
static void insertUnique(ccRBTree_t* into, ccRBTree_t* set);

static inline ccList_t* listSetKeys(ccRBTree_t* set);

static void dbg_printTransition(dfa_transition_t* transition);
static void dbg_printTransitionList(ccList_t* list);
static void dbg_printStates(dfa_t* dfa);
static void dbg_printSetData(void* data);
static void dbg_printAuxCharSet(void* data);
static void dbg_printListOfSets(ccList_t* S, void (*printData)(void*));
static void dbg_printSetOfStatesDataVariant(ccRBTree_t* S, void (*printData)(void*));
static void dbg_printSetOfStatesKeyVariant(ccRBTree_t* S, void (*printData)(void*));

static int compareSetsImpl(ccRBTree_t* setA, ccRBTree_t* setB, ccRBTreeNode_t* a, ccRBTreeNode_t* b, int(*compare_fn)(void*, void*));
static int compareStateSets(void* a, void* b);
static int compareStates(void* a, void* b);
static int compareSets(void* a, void* b);
static int compareByChar(void* a, void* b);
static int compareReferences(void* a, void* b);

dfa_state_t* dfa_state_ctor(stateType_t state, ccRBTree_t* set);
dfa_transition_t* dfa_transition_ctor(dfa_state_t* fromState, dfa_state_t* toState, char isComplement, char symbol);
dfa_t* dfa_ctor();

void dfa_dtor(dfa_t* dfa);
void dfa_state_dtor(dfa_state_t* data);
void dfa_transition_dtor(dfa_transition_t* data);

dfa_t* buildDFA(nfa_t* nfa)
{
    bool hasAcceptState = false;
    stateType_t stateId = 0;
    ccStack_t* workStack = ccStack_ctor(256, NULL);
    ccStack_t* walkStack = ccStack_ctor(256, NULL);
    ccRBTree_t* stateSet = ccRBTree_ctor(compareStateSets);
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
    ccList_append(dfa->setList, ccListNode_ctor(fromSet, NULL));
    fromState = dfa_state_ctor(stateId, fromSet);
    ccList_append(dfa->states, ccListNode_ctor(fromState, (dtor_t)dfa_state_dtor));
    dfa->start = fromState->state;
    dfa->startState = fromState;
    stateId += 1;
    ccRBTree_insert(stateSet, ccRBTreeNode_ctor(fromState, fromState, NULL, NULL));
    ccStack_push(workStack, fromState);
    while(workStack->size != 0){
        fromState = ccStack_pop(workStack);
        ccStack_push(walkStack, fromState->set->head);
        while(walkStack->size != 0){
            hasAcceptState = false;
            setNode = ccStack_pop(walkStack);
            if(!ccRBTree_isLeaf(fromState->set, setNode->left)){
                ccStack_push(walkStack, setNode->left);
            }
            if(!ccRBTree_isLeaf(fromState->set, setNode->right)){
                ccStack_push(walkStack, setNode->right);
            }
            node = *(ccListNode_t**)ccDynamicArray_get(nfa->states, *(size_t*)(setNode->data));
            if(node != NULL){
                nfa_transition = (nfa_transition_t*)node->data;
                if(!nfa_transition->isEpsilon){
                    toSet = followEpsilon(nfa, &nfa_transition->toState, &hasAcceptState);
                    ccList_append(dfa->setList, ccListNode_ctor(toSet, NULL));
                    auxState = dfa_state_ctor(stateId, toSet);
                    auxSetNode = ccRBTree_find(stateSet, auxState);
                    if(auxSetNode == NULL){
                        toState = dfa_state_ctor(stateId, toSet);
                        ccList_append(dfa->states, ccListNode_ctor(toState, (dtor_t)dfa_state_dtor));
                        stateId += 1;
                        ccRBTree_insert(stateSet, ccRBTreeNode_ctor(toState, toState, NULL, NULL));
                        ccStack_push(workStack, toState);
                        free(auxState);
                    }else{
                        toState = (dfa_state_t*)(auxSetNode->data);
                        dfa_state_dtor(auxState);
                    }
                    dfa_transition = dfa_transition_ctor(fromState, toState, nfa_transition->isComplement, nfa_transition->symbol);
                    /* insert transition into ccDynamicArray_t* transitions; */
                    list = *(ccList_t**)ccDynamicArray_get(dfa->transitions, dfa_transition->fromState->state);
                    if(list == NULL){
                        list = ccList_ctor();
                        ccDynamicArray_set(dfa->transitions, dfa_transition->fromState->state, &list);
                    }
                    ccList_append(list, ccListNode_ctor(dfa_transition, NULL));
                    /* if transition contains acceptState of nfa, add it to ccRBTree_t* acceptStates; */
                    if(hasAcceptState && !ccRBTree_contains(dfa->acceptStates, &dfa_transition->toState->state))
                        ccRBTree_insert(dfa->acceptStates, ccRBTreeNode_ctor(&dfa_transition->toState->state, &dfa_transition->toState->state, NULL, NULL));
                }
            }
        }
    }

    (void)dbg_printSetOfStatesKeyVariant;
    (void)listSetKeys;
    (void)dbg_printStates;
    (void)dbg_printSet;
    (void)dbg_printSet;
    (void)dbg_printAuxCharSet;
    ccStack_dtor(workStack);
    ccStack_dtor(walkStack);
    ccRBTree_dtor(stateSet);

    return dfa;
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
    ccRBTree_t* set = ccRBTree_ctor(compareStates);
    
    if(*stateIndex == ((nfa_transition_t*)(nfa->accept->data))->toState)
        *hasAcceptState = true;
    ccRBTree_insert(set, ccRBTreeNode_ctor(stateIndex, stateIndex, NULL, NULL));
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
                ccRBTree_insert(set, ccRBTreeNode_ctor(&((nfa_transition_t*)state->data)->toState, &((nfa_transition_t*)state->data)->toState, NULL, NULL));
                if(((nfa_transition_t*)state->data)->toState == ((nfa_transition_t*)(nfa->accept->data))->toState)
                    *hasAcceptState = true;
            }
            state = state->next;
        }
    }
    ccStack_dtor(stack);

    return set;
}

static inline ccList_t* listSetData(ccRBTree_t* set)
{
    ccList_t* ret = ccList_ctor();
    ccRBTreeNode_t* node;
    ccStack_t* stack = ccStack_ctor(256, NULL);

    if(!ccRBTree_isLeaf(set, set->head))
        ccStack_push(stack, set->head);
    while(stack->size != 0){
        node = ccStack_pop(stack);
        if(!ccRBTree_isLeaf(set, node->left))
            ccStack_push(stack, node->left);
        if(!ccRBTree_isLeaf(set, node->right))
            ccStack_push(stack, node->right);
        ccList_append(ret, ccListNode_ctor(node->data, NULL));
    }
    ccStack_dtor(stack);

    return ret;
}

static inline ccList_t* listSetKeys(ccRBTree_t* set)
{
    ccList_t* ret = ccList_ctor();
    ccRBTreeNode_t* node;
    ccStack_t* stack = ccStack_ctor(256, NULL);

    if(!ccRBTree_isLeaf(set, set->head))
        ccStack_push(stack, set->head);
    while(stack->size != 0){
        node = ccStack_pop(stack);
        if(!ccRBTree_isLeaf(set, node->left))
            ccStack_push(stack, node->left);
        if(!ccRBTree_isLeaf(set, node->right))
            ccStack_push(stack, node->right);
        ccList_append(ret, ccListNode_ctor(node->key, NULL));
    }
    ccStack_dtor(stack);

    return ret;
}

ccRBTree_t* getToSet(ccRBTree_t* auxStateSet, dfa_transition_t* t)
{
    return ((dfa_state_t*)(ccRBTree_find(auxStateSet, &t->toState->state)->data))->set;
}

ccRBTree_t* getFromSet(ccRBTree_t* auxStateSet, dfa_transition_t* t)
{
    return ((dfa_state_t*)(ccRBTree_find(auxStateSet, &t->fromState->state)->data))->set;
}

static void insertUnique(ccRBTree_t* into, ccRBTree_t* set)
{
    if(!ccRBTree_contains(into, set))
        ccRBTree_insert(into, ccRBTreeNode_ctor(NULL, set, NULL, (dtor_t)ccRBTree_dtor));
}

void splitSets(dfa_t* minDFA, ccRBTree_t* auxStateSet, ccList_t* transitionList, ccRBTree_t* from, ccRBTree_t* to)
{
    ccRBTree_t* s = ccRBTree_ctor(compareStates);
    ccRBTreeNode_t* node;
    dfa_state_t* auxState;
    dfa_transition_t* transition;

    for(size_t ti = 0; ti < transitionList->size; ++ti){
        transition = ccList_itemAt(transitionList, ti);
        if(ccRBTree_contains(from, &transition->fromState->state) 
            && ccRBTree_contains(to, &transition->toState->state) 
            && !ccRBTree_contains(s, &transition->fromState->state)){
            node = ccRBTree_find(from, &transition->fromState->state);
            ccRBTree_removeNode(from, node);
            ccRBTreeNode_dtor(node);
            ccRBTree_insert(s, ccRBTreeNode_ctor(&transition->fromState->state, &transition->fromState->state, NULL, NULL));
            auxState = dfa_state_ctor(0, s);
            ccList_append(minDFA->states, ccListNode_ctor(auxState, free));
            node = ccRBTree_find(auxStateSet, &transition->fromState->state);
            ccRBTree_removeNode(auxStateSet, node);
            ccRBTreeNode_dtor(node);
            ccRBTree_insert(auxStateSet, ccRBTreeNode_ctor(auxState, &transition->fromState->state, NULL, NULL));
        }
    }
    insertUnique(minDFA->stateSet, s);
}

ccRBTree_t* createUniquesSet(dfa_t* dfa, ccRBTree_t* auxStateSet, stateType_t* stateIndex)
{
    ccRBTree_t* s = ccRBTree_ctor(compareReferences);
    ccRBTreeNode_t* node;
    dfa_state_t* state;
    ccStack_t* stack = ccStack_ctor(256, NULL);

    if(!ccRBTree_isLeaf(auxStateSet, auxStateSet->head))
        ccStack_push(stack, auxStateSet->head);
    while(stack->size != 0){
        node = ccStack_pop(stack);
        if(!ccRBTree_isLeaf(auxStateSet, node->left))
            ccStack_push(stack, node->left);
        if(!ccRBTree_isLeaf(auxStateSet, node->right))
            ccStack_push(stack, node->right);
        if(!ccRBTree_contains(s, ((dfa_state_t*)node->data)->set)){
            state = dfa_state_ctor(*stateIndex, NULL);
            ccRBTree_insert(s, ccRBTreeNode_ctor(state, ((dfa_state_t*)node->data)->set, NULL, NULL));
            *stateIndex += 1;
            ccList_append(dfa->states, ccListNode_ctor(state, free));
        }
    }
    ccStack_dtor(stack);

    return s;
}

/* if only we created just one more set ... */
void ccList_addUniqueTransition(ccList_t* list, dfa_transition_t* transition)
{
    dfa_transition_t* aux;
    bool unique = true;
    for(size_t i = 0; i < list->size; ++i){
        aux = ccList_itemAt(list, i);
        if(aux->toState->state != transition->toState->state)
            continue;
        if(aux->symbol != transition->symbol)
            continue;
        if(aux->isComplement != transition->isComplement)
            continue;

        unique = false;
        break;
    }

    if(unique)
        ccList_append(list, ccListNode_ctor(transition, NULL));
    else
        free(transition);
}

void fixupStates(dfa_t* minimizedDFA, dfa_t* dfa, ccRBTree_t* auxStateSet, ccRBTree_t* states)
{
    bool fixedStart = false;
    ccList_t* list;
    ccList_t* auxList;
    dfa_state_t* fromState;
    dfa_state_t* toState;
    dfa_state_t* state;
    ccRBTree_t* set;
    stateType_t fromStateIndex;
    stateType_t toStateIndex;
    dfa_transition_t* transition;
    dfa_transition_t* newTransition;
    ccRBTreeNode_t* treeNode;

    minimizedDFA->start = dfa->start;
    for(size_t i = 0; i < dfa->transitions->size; ++i){
        list = *(ccList_t**)ccDynamicArray_get(dfa->transitions, i);
        if(list == NULL)
            continue;
        for(size_t j = 0; j < list->size; ++j){
            transition = ccList_itemAt(list, j);
            /* should not be NULL, if it is NULL we have a bug in our code */
            treeNode = ccRBTree_find(auxStateSet, &transition->toState->state);
            set = ((dfa_state_t*)(treeNode->data))->set;
            treeNode = ccRBTree_find(states, set);
            state = (dfa_state_t*)(treeNode->data);
            toStateIndex = state->state;
            treeNode = ccRBTree_find(auxStateSet, &transition->fromState->state);
            set = ((dfa_state_t*)(treeNode->data))->set;
            treeNode = ccRBTree_find(states, set);
            state = (dfa_state_t*)(treeNode->data);
            fromStateIndex = state->state;
            fromState = dfa_state_ctor(fromStateIndex, NULL);
            toState = dfa_state_ctor(toStateIndex, NULL);
            ccList_append(dfa->states, ccListNode_ctor(fromState, free));
            ccList_append(dfa->states, ccListNode_ctor(toState, free));
            newTransition = dfa_transition_ctor(fromState, 
                toState, transition->isComplement, 
                transition->symbol);
            auxList = *(ccList_t**)ccDynamicArray_get(minimizedDFA->transitions, fromStateIndex);
            if(auxList == NULL){
                auxList = ccList_ctor();
                ccDynamicArray_set(minimizedDFA->transitions, fromStateIndex, &auxList);
            }
            ccList_addUniqueTransition(auxList, newTransition);
            if(fixedStart == false){
                if(transition->fromState->state == dfa->start){
                    minimizedDFA->start = fromStateIndex;
                    fixedStart = true;
                }
            }
            if(ccRBTree_contains(dfa->acceptStates, &transition->toState->state)
                && !ccRBTree_contains(minimizedDFA->acceptStates, &toStateIndex))
                ccRBTree_insert(minimizedDFA->acceptStates, ccRBTreeNode_ctor(&toState->state, &toState->state, NULL, NULL));
        }
    }
}

dfa_t* minimizeDFA(dfa_t* dfa)
{
    dfa_t* minimizedDFA = dfa_ctor();
    ccRBTree_t* auxStateSet = ccRBTree_ctor(compareStates);
    ccRBTree_t* fromA = ccRBTree_ctor(compareStates);
    ccRBTree_t* fromB = ccRBTree_ctor(compareStates);
    ccRBTree_t* toA;
    ccRBTree_t* toB;
    ccRBTree_t* saux;
    dfa_state_t* state;
    dfa_state_t* auxState;
    dfa_transition_t* a;
    dfa_transition_t* b;
    ccList_t* setList;
    ccList_t* transitionList;
    ccList_t* workList;
    ccList_t* stateList;
    ccRBTree_t* states;
    stateType_t stateIndex = 0;
    stateType_t auxStateIndex = 0;
    bool changed = true;

    for(size_t i = 0; i < dfa->states->size; ++i){
        state = ccList_itemAt(dfa->states, i);
        if(ccRBTree_contains(dfa->acceptStates, &state->state))
            saux = fromA;
        else
            saux = fromB;
        ccRBTree_insert(saux, ccRBTreeNode_ctor(&state->state, &state->state, NULL, NULL));
        auxState = dfa_state_ctor(0, saux);
        ccList_append(minimizedDFA->states, ccListNode_ctor(auxState, free));
        ccRBTree_insert(auxStateSet, ccRBTreeNode_ctor(auxState, &state->state, NULL, NULL));
    }
    insertUnique(minimizedDFA->stateSet, fromA);
    insertUnique(minimizedDFA->stateSet, fromB);
    while(changed){
        changed = false;
        setList = listSetKeys(minimizedDFA->stateSet);
        for(size_t setIndex = 0;  setIndex < setList->size; ++setIndex){
            workList = ccList_ctor();
            stateList = listSetData(((ccRBTree_t*)ccList_itemAt(setList, setIndex)));
            for(size_t i = 0;  i < stateList->size; ++i){
                auxStateIndex = *(stateType_t*)ccList_itemAt(stateList, i);
                transitionList = *(ccList_t**)ccDynamicArray_get(dfa->transitions, auxStateIndex);
                if(transitionList == NULL)
                    continue;
                for(size_t j = 0; j < transitionList->size; ++j){
                    a = ccList_itemAt(transitionList, j);
                    ccList_append(workList, ccListNode_ctor(a, NULL));
                }
            }
            for(size_t i = 0; i < workList->size; ++i){
                a = ccList_itemAt(workList, i);
                for(size_t j = i + 1; j < workList->size; ++j){
                    b = ccList_itemAt(workList, j);
                    if(a->symbol != b->symbol)
                        continue;
                    if(a->fromState == b->fromState)
                        continue;
                    fromA = getFromSet(auxStateSet, a);
                    fromB = getFromSet(auxStateSet, b);
                    if(fromA != fromB)
                        continue;
                    toA = getToSet(auxStateSet, a);
                    toB = getToSet(auxStateSet, b);
                    if(toA == toB)
                        continue;
                    splitSets(minimizedDFA, auxStateSet, workList, fromA, toA);
                    splitSets(minimizedDFA, auxStateSet, workList, fromB, toB);
                    changed = true;
                }
            }
            ccList_dtor(workList);
            ccList_dtor(stateList);
        }
        ccList_dtor(setList);
    }
    states = createUniquesSet(minimizedDFA, auxStateSet, &stateIndex);
    fixupStates(minimizedDFA, dfa, auxStateSet, states);

    (void)dbg_printStates;
    (void)dbg_printSet;
    (void)dbg_printSetOfStatesDataVariant;
    (void)dbg_printSetData;
    (void)compareByChar;
    (void)compareSets;
    (void)dbg_printListOfSets;

    ccRBTree_dtor(auxStateSet);
    ccRBTree_dtor(states);

    return minimizedDFA;
}

static inline int doFollow(ccRBTree_t* set, nfa_transition_t* transition)
{
    if(transition->isEpsilon && !ccRBTree_contains(set, &transition->toState))
        return 1;
    return 0;
}

static inline int hasDescendants(ccRBTree_t* set, ccRBTreeNode_t* a)
{
    if(a->left == set->null && a->right == set->null)
        return 0;
    return 1;
}

static void dbg_printTransition(dfa_transition_t* transition)
{
    ccLogDebug(
        "\n\tfromState: \t%ld\n"
        "\ttoState: \t%ld\n"
        "\tisCompl: \t%s\n"
        "\tsymbol: \t%c (0x%x)\n",
        transition->fromState->state,
        transition->toState->state,
        (int)transition->isComplement ? "True" : "False",
        transition->symbol,
        transition->symbol
    );
}

static void dbg_printTransitionList(ccList_t* list)
{
    dfa_transition_t* transition = NULL;

    ccLogDebug("LINKAGE START");
    for(size_t j = 0; j < list->size; ++j){
        transition = (dfa_transition_t*)ccList_itemAt(list, j);
        dbg_printTransition(transition);
    }
    ccLogDebug("LINKAGE END");
}

static void dbg_printStates(dfa_t* dfa)
{
    ccList_t* list = NULL;

    for(size_t i = 0; i < dfa->transitions->size; ++i){
        list = *(ccList_t**)ccDynamicArray_get(dfa->transitions, i);
        if(list == NULL)
            continue;
        dbg_printTransitionList(list);
    }
}

static void dbg_printSetData(void* data)
{
    int state = *(int*)(((ccRBTreeNode_t*)data)->data);
    ccLogDebug("data: %d ", state);
}

static void dbg_printAuxCharSet(void* data)
{
    ccRBTreeNode_t* node = (ccRBTreeNode_t*)data;
    ccLogDebug("START Printing tree node");
    ccLogDebug("key %c", *(char*)node->key);
    dbg_printTransitionList((node->data));
    ccLogDebug("END Printing tree node");
}

static void dbg_printListOfSets(ccList_t* S, void (*printData)(void*))
{
    ccRBTree_t* set;

    for(size_t i = 0; i < S->size; ++i){
        ccLogDebug("\n\nSet at index: %ld", i);
        set = ccList_itemAt(S, i);
        dbg_printSet(set, printData);
    }
}

static void dbg_printSetOfStatesDataVariant(ccRBTree_t* S, void (*printData)(void*))
{
    ccStack_t* stack = ccStack_ctor(256, NULL);
    ccRBTreeNode_t* node;
    size_t index = 0;

    if(!ccRBTree_isLeaf(S, S->head))
        ccStack_push(stack, S->head);
    while(stack->size != 0){
        node = ccStack_pop(stack);
        if(!ccRBTree_isLeaf(S, node->left))
            ccStack_push(stack, node->left);
        if(!ccRBTree_isLeaf(S, node->right))
            ccStack_push(stack, node->right);
        ccLogDebug("\nSet [%ld]:[%ld] addr [%p]", index, *(stateType_t*)node->key, ((dfa_state_t*)node->data)->set);
        dbg_printSet(((dfa_state_t*)(node->data))->set, printData);

        index+=1;
    }
}

static void dbg_printSetOfStatesKeyVariant(ccRBTree_t* S, void (*printData)(void*))
{
    ccStack_t* stack = ccStack_ctor(256, NULL);
    ccRBTreeNode_t* node;
    size_t index = 0;

    if(!ccRBTree_isLeaf(S, S->head))
        ccStack_push(stack, S->head);
    while(stack->size != 0){
        node = ccStack_pop(stack);
        if(!ccRBTree_isLeaf(S, node->left))
            ccStack_push(stack, node->left);
        if(!ccRBTree_isLeaf(S, node->right))
            ccStack_push(stack, node->right);
        ccLogDebug("\nSet [%ld] addr [%p]", index, ((dfa_state_t*)node->key)->set);
        dbg_printSet((node->key), printData);

        index+=1;
    }
}

void dbg_printDFA(dfa_t* dfa)
{
    ccLogDebug("Transitions");
    dbg_printStates(dfa);
    ccLogDebug("Accept States:");
    dbg_printSet(dfa->acceptStates, dbg_printSetData);
    ccLogDebug("Start state: %ld", dfa->start);
}

static int compareReferences(void* a, void* b)
{
    return (a - b);
}

static int compareSetsImpl(ccRBTree_t* setA, ccRBTree_t* setB, ccRBTreeNode_t* a, ccRBTreeNode_t* b, int(*compare_fn)(void*, void*))
{
    int status = 0;
    int leafA = ccRBTree_isLeaf(setA, a);
    int leafB = ccRBTree_isLeaf(setB, b);

    if(leafA == 1 || leafB == 1)
        return leafA - leafB;

    if(!hasDescendants(setA, a) && !hasDescendants(setB, b))
        return compare_fn(a->key, b->key);

    status = compareSetsImpl(setA, setB, a->left, b->left, compare_fn);
    if(status == 0)
        status = compareSetsImpl(setA, setB, a->right, b->right, compare_fn);

    return status;    
}

static int compareStateSets(void* a, void* b)
{
    ccRBTree_t* A = ((dfa_state_t*)a)->set;
    ccRBTree_t* B = ((dfa_state_t*)b)->set;

    if(A == B)
        return 0;

    return compareSetsImpl(A, B, A->head, B->head, compareStates);
}

static int compareSets(void* a, void* b)
{
    ccRBTree_t* A = (ccRBTree_t*)a;
    ccRBTree_t* B = (ccRBTree_t*)b;

    if(A == B)
        return 0;

    return compareSetsImpl(A, B, A->head, B->head, compareStates);
}

static int compareStates(void* a, void* b)
{
    stateType_t A, B;
    
    A = *(stateType_t*)a;
    B = *(stateType_t*)b;

    return (int)(A-B);
}

static int compareByChar(void* a, void* b)
{
    char A = *(char*)a;
    char B = *(char*)b;

    return A - B;
}

dfa_state_t* dfa_state_ctor(stateType_t state, ccRBTree_t* set)
{
    dfa_state_t* newState;
    expectExit(newState, malloc(sizeof(dfa_state_t)), != NULL);
    newState->state = state;
    newState->set = set;

    ccType_ctor(&newState->type, "dfa_state_t");

    return newState;
}

dfa_transition_t* dfa_transition_ctor(dfa_state_t* fromState, dfa_state_t* toState, char isComplement, char symbol)
{
    dfa_transition_t* transition;
    
    expectExit(transition, malloc(sizeof(dfa_transition_t)), != NULL);
    transition->fromState = fromState;
    transition->toState = toState;
    transition->isComplement = isComplement;
    transition->symbol = symbol;

    ccType_ctor(&transition->type, "dfa_transition_t");

    return transition;
}

dfa_t* dfa_ctor()
{
    dfa_t* dfa;
    expectExit(dfa, malloc(sizeof(dfa_t)), != NULL);

    dfa->start = 0;
    dfa->acceptStates = ccRBTree_ctor(compareStates);
    dfa->transitions = ccDynamicArray_ctor(sizeof(ccList_t*), true);
    dfa->setList = ccList_ctor();
    dfa->states = ccList_ctor();
    dfa->stateSet = ccRBTree_ctor(compareReferences);

    return dfa;
}

void dfa_dtor(dfa_t* dfa)
{
    ccList_t* list;
    dfa_transition_t* transition;

    ccRBTree_dtor(dfa->acceptStates);
    for(size_t i = 0; i < dfa->transitions->size; ++i){
        list = *(ccList_t**)ccDynamicArray_get(dfa->transitions, i);
        if(list == NULL)
            continue;
        for(size_t j = 0; j < list->size; ++j){
            transition = ccList_itemAt(list, j);
            dfa_transition_dtor(transition);
        }
        ccList_dtor(list);
    }
    ccDynamicArray_dtor(dfa->transitions);
    ccList_dtor(dfa->setList);
    ccList_dtor(dfa->states);
    ccRBTree_dtor(dfa->stateSet);
    free(dfa);
}

void dfa_state_dtor(dfa_state_t* data)
{
    ccRBTree_dtor(data->set);
    free(data);
}

void dfa_transition_dtor(dfa_transition_t* data)
{
    free(data);
}