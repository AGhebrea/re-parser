#include "ccDynamicArray.h"
#include "ccList.h"
#include "cclog_macros.h"
#include "parserTypes.h"
#include <nfa.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <cclog.h>

// #define TRACEDEBUG

/* TODO: interleave the NFA build code with the parsing code. */
/* NOTE: A general thing that i keep thinking about is if there are any ways to represent 
 * more complex data structures in a flatter manner and not have to dereference more than 
 * 1-2 times to get to values. Also, isn't there a simpler way to shuffle data around like 
 * this ? */

void opComplement(stateType_t* indexState, nfa_t* a);
void opConcatenation(stateType_t* indexState, nfa_t* a, nfa_t* b);
void opAlternation(stateType_t* indexState, nfa_t* a, nfa_t* b);
void opKleene(stateType_t* indexState, nfa_t* a);
void opPositiveClosure(stateType_t* indexState, nfa_t* a);
void opExplicitClosure(stateType_t* indexState, nfa_t* a, size_t times);

nfa_t* buildNFA_Epsilon(stateType_t* indexState);
nfa_t* buildNFA_Character(stateType_t* indexState, char expression);
nfa_t* buildNFA_CaptureGroup(stateType_t* indexState, expressionCaptureGroup_t* expression);
nfa_t* buildNFA_Term(stateType_t* indexState, expressionTerm_t* expression);
nfa_t* buildNFA_Complement(stateType_t* indexState, expressionComplement_t* expression);
nfa_t* buildNFA_Closure(stateType_t* indexState, expressionClosure_t* expression);
nfa_t* buildNFA_Concatenation(stateType_t* indexState, expressionConcatenation_t* expression);
nfa_t* buildNFA_Alternation(stateType_t* indexState, expressionAlternation_t* expression);
nfa_t* buildNFA_RegularExpression(stateType_t* indexState, expressionRegularExpression_t* expression);

void nfa_shallowDtor(nfa_t* data);

void dbg_printNFA(nfa_t* nfa);
void dbg_printNFAStates(nfa_t* nfa);

void fixupStates(nfa_t* nfa);

nfa_t* copyNFA(stateType_t* indexState, nfa_t* nfa){
    ccListNode_t* node = NULL;
    nfa_transition_t* transition = NULL;
    nfa_transition_t* start = nfa->start->data;
    nfa_transition_t* accept = nfa->accept->data;
    nfa_t* newNFA = nfa_ctor();
    ccList_t* newList = ccList_ctor();
    ccList_t* list = nfa->transitions;
    stateType_t index = *indexState;

    for(size_t i = 0; i < list->size; ++i){
        transition = (nfa_transition_t*)ccList_itemAt(list, i);
        node = ccListNode_ctor(nfa_transition_ctor(
            index + transition->fromState, index + transition->toState, transition->isComplement, 
            transition->isEpsilon, transition->symbol), free);
        ccList_append(newList, node);
        if(transition == start){
            newNFA->start = node;
        }else if(transition == accept){
            newNFA->accept = node;
        }
    }

    *indexState += list->size + 1;

    return newNFA;
}

/* To complement a regular expression (RE), for every non-epsilon transition in the original RE, 
 * we add a new transition from the same starting state to an auxiliary state. This new transition 
 * is made on the complement of the original symbol. From each of these auxiliary states, we then 
 * create an epsilon transition to a new acknowledgment (ACK) state. Essentially, this process 
 * marks all states from the original RE, except its original ACK state, as valid acknowledgment 
 * states in the complemented RE. */
void opComplement(stateType_t* indexState, nfa_t* a)
{
    ccLogTrace();
    bool appendedAck = false;
    ccListNode_t* ackTransition = NULL;
    nfa_transition_t* transition = NULL;
    size_t size = a->transitions->size;
    stateType_t ackIndex = *indexState;
    *indexState += 1;

    /* TODO: what if there are only epsilon transitions ? do we allow that ? 
     * sort that out */
    /* TODO: when we unlink, we might drop an entire graph, we should look into the effects of that */
    for(size_t i = 0; i < size; ++i){
        transition = (nfa_transition_t*)ccList_itemAt(a->transitions, i);
        if(transition->isEpsilon != 0)
            continue;
        ccList_append(a->transitions, ccListNode_ctor(nfa_transition_ctor( 
            transition->fromState, (*indexState), 1, 0, transition->symbol), free));
        if(appendedAck == false){
            ackTransition = ccListNode_ctor(nfa_transition_ctor( (*indexState), 
                ackIndex, 0, 1, '?'), free);
            ccList_append(a->transitions, ackTransition);
            appendedAck = true;
        }
    }

    *indexState += 1;
    ccList_unlink(a->transitions, a->accept);
    if(a->accept == a->start){
        a->start = ackTransition;
    }
    ccListNode_dtor(a->accept);
    a->accept = ackTransition;
}


void opConcatenation(stateType_t* indexState, nfa_t* a, nfa_t* b)
{
    ccLogTrace();
    nfa_transition_t* transition_a = NULL;
    nfa_transition_t* transition_b = NULL;

    (void)indexState;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Before");
    ccLogDebug("A");
    dbg_printNFA(a);
    ccLogDebug("B");
    dbg_printNFA(b);
#endif // TRACEDEBUG

    ccList_join(a->transitions, b->transitions);

    transition_a = (nfa_transition_t*)a->accept->data;
    transition_b = (nfa_transition_t*)b->start->data;

    ccList_append(a->transitions, ccListNode_ctor(nfa_transition_ctor( 
        transition_a->toState, transition_b->fromState, 0, 1, '?'), free));
    a->accept = b->accept;

    nfa_shallowDtor(b);

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Result");
    dbg_printNFA(a);
#endif // TRACEDEBUG
}

void opAlternation(stateType_t* indexState, nfa_t* a, nfa_t* b)
{
    ccLogTrace();
    nfa_transition_t* transition = NULL;
    ccListNode_t* node = NULL;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Before");
    ccLogDebug("A");
    dbg_printNFA(a);
    ccLogDebug("B");
    dbg_printNFA(b);
#endif // TRACEDEBUG

    ccList_join(a->transitions, b->transitions);
    transition = (nfa_transition_t*)a->start->data;
    node = ccListNode_ctor(nfa_transition_ctor( *indexState, transition->fromState, 0, 1, '?'), free);
    ccList_append(a->transitions, node);
    transition = (nfa_transition_t*)b->start->data;
    node = ccListNode_ctor(nfa_transition_ctor( *indexState, transition->fromState, 0, 1, '?'), free);
    ccList_append(a->transitions, node);
    a->start = node;
    *indexState += 1;

    transition = (nfa_transition_t*)a->accept->data;
    node = ccListNode_ctor(nfa_transition_ctor( transition->toState, *indexState, 0, 1, '?'), free);
    ccList_append(a->transitions, node);
    transition = (nfa_transition_t*)b->accept->data;
    node = ccListNode_ctor(nfa_transition_ctor( transition->toState, *indexState, 0, 1, '?'), free);
    ccList_append(a->transitions, node);
    a->accept = node;
    *indexState += 1;

    nfa_shallowDtor(b);

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Result");
    dbg_printNFA(a);
#endif // TRACEDEBUG
}

void opKleene(stateType_t* indexState, nfa_t* a)
{
    ccLogTrace();
    nfa_transition_t* ack = NULL;
    nfa_transition_t* start = NULL;
    ccListNode_t* node = NULL;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Before");
    ccLogDebug("A");
    dbg_printNFA(a);
#endif // TRACEDEBUG

    ack = (nfa_transition_t*)a->accept->data;
    start = (nfa_transition_t*)a->start->data;
    node = ccListNode_ctor(nfa_transition_ctor(ack->toState, *indexState, 0, 1, '?'), free);
    ccList_append(a->transitions, node);
    a->accept = node;
    ccList_append(a->transitions, ccListNode_ctor(nfa_transition_ctor(start->fromState, *indexState, 0, 1, '?'), free));
    ccList_append(a->transitions, ccListNode_ctor(nfa_transition_ctor(ack->toState, start->fromState, 0, 1, '?'), free));

    *indexState += 1;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Result");
    dbg_printNFA(a);
#endif // TRACEDEBUG
}

void opPositiveClosure(stateType_t* indexState, nfa_t* a)
{
    ccLogTrace();
    nfa_transition_t* ack = NULL;
    nfa_transition_t* start = NULL;
    ccListNode_t* node = NULL;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Before");
    ccLogDebug("A");
    dbg_printNFA(a);
#endif // TRACEDEBUG

    ack = (nfa_transition_t*)a->accept->data;
    start = (nfa_transition_t*)a->start->data;
    node = ccListNode_ctor(nfa_transition_ctor(ack->toState, *indexState, 0, 1, '?'), free);
    ccList_append(a->transitions, node);
    a->accept = node;
    ccList_append(a->transitions, ccListNode_ctor(nfa_transition_ctor(ack->toState, start->fromState, 0, 1, '?'), free));

    *indexState += 1;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Result");
    dbg_printNFA(a);
#endif // TRACEDEBUG
}

void opExplicitClosure(stateType_t* indexState, nfa_t* a, size_t times)
{
    ccLogTrace();
    nfa_t* aux = a;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Before");
    ccLogDebug("A");
    dbg_printNFA(a);
#endif // TRACEDEBUG

    if(times <= 1)
        return;

    for(size_t i = 1; i < times; ++i){
        aux = copyNFA(indexState, aux);
        opConcatenation(indexState, a, aux);
    }

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Result");
    dbg_printNFA(a);
#endif // TRACEDEBUG
}

nfa_t* buildNFA_Epsilon(stateType_t* indexState)
{
    nfa_t* nfa_result = nfa_ctor();
    ccListNode_t* node;
    
    node = ccListNode_ctor(nfa_transition_ctor(*indexState, *indexState + 1, 0, 1, '?'), free);
    ccList_append(nfa_result->transitions, node);
    nfa_result->start = node;
    nfa_result->accept = node;
    *indexState += 2;

    return nfa_result;
}

nfa_t* buildNFA_Character(stateType_t* indexState, char expression)
{
    ccLogTrace();
    nfa_t* nfa_result = nfa_ctor();
    ccListNode_t* node;
    
    node = ccListNode_ctor(nfa_transition_ctor(*indexState, *indexState + 1, 0, 0, expression), free);
    ccList_append(nfa_result->transitions, node);
    nfa_result->start = node;
    nfa_result->accept = node;
    *indexState += 2;

    return nfa_result;
}

nfa_t* buildNFA_CaptureGroup(stateType_t* indexState, expressionCaptureGroup_t* expression)
{
    ccLogTrace();
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_aux = NULL;

    nfa_result = buildNFA_Character(indexState, (char)expression->leftCharacter);
    for(size_t i = expression->leftCharacter + 1; i < expression->rightCharacter; ++i){
        nfa_aux = buildNFA_Character(indexState, (char)i);
        opAlternation(indexState, nfa_result, nfa_aux);
    }

    return nfa_result;
}

nfa_t* buildNFA_Term(stateType_t* indexState, expressionTerm_t* expression)
{
    ccLogTrace();
    nfa_t* nfa_result = NULL;

    switch(expression->type){
    case typeTerm_RegularExpression:
        nfa_result = buildNFA_RegularExpression(indexState, expression->value.regularExpression);
        break;
    case typeTerm_CaptureGroup:
        nfa_result = buildNFA_CaptureGroup(indexState, expression->value.captureGroup);
        break;
    case typeTerm_Character:
        nfa_result = buildNFA_Character(indexState, expression->value.character);
        break;
    case typeTerm_Epsilon:
        nfa_result = buildNFA_Epsilon(indexState);
        break;
    default:
        ccLogError("Default case should be unreachable!");
        break;
    }

    return nfa_result;
}

nfa_t* buildNFA_Complement(stateType_t* indexState, expressionComplement_t* expression)
{
    ccLogTrace();
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_term = NULL;

    nfa_term = buildNFA_Term(indexState, expression->term);
    switch(expression->type){
    case typeComplement_Complement:
        opComplement(indexState, nfa_term);
        break;
    case typeComplement_Term:
        break;
    default:
        ccLogError("Default case should be unreachable!");
        break;
    }

    nfa_result = nfa_term;

    return nfa_result;
}

nfa_t* buildNFA_Closure(stateType_t* indexState, expressionClosure_t* expression)
{
    ccLogTrace();
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_complement = NULL;

    nfa_complement = buildNFA_Complement(indexState, expression->complement);
    switch(expression->type){
    case typeClosure_Kleene:
        opKleene(indexState, nfa_complement);
        break;
    case typeClosure_Positive:
        opPositiveClosure(indexState, nfa_complement);
        break;
    case typeClosure_Explicit:
        opExplicitClosure(indexState, nfa_complement, expression->repetitionNumber);
        break;
    case typeClosure_Complement:
        break;
    default:
        ccLogError("Default case should be unreachable!");
        break;
    }
    nfa_result = nfa_complement;

    return nfa_result;
}

nfa_t* buildNFA_Concatenation(stateType_t* indexState, expressionConcatenation_t* expression)
{
    ccLogTrace();
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_concatenation = NULL;
    nfa_t* nfa_closure = NULL;

    switch(expression->type){
    case typeConcatenation_ConcatentationClosure:
        nfa_concatenation = buildNFA_Concatenation(indexState, expression->concatenation);
        nfa_closure = buildNFA_Closure(indexState, expression->closure);
        // opConcatenation(indexState, nfa_concatenation, nfa_closure);
        // nfa_result = nfa_concatenation;
        opConcatenation(indexState, nfa_closure, nfa_concatenation);
        nfa_result = nfa_closure;
        break;
    case typeConcatenation_Closure:
        nfa_result = buildNFA_Closure(indexState, expression->closure);
        break;
    case typeConcatenation_NULL:
    case typeConcatenation_Empty:
        break;
    default:
        ccLogError("Default case should be unreachable!");
        break;
    }

    return nfa_result;
}

nfa_t* buildNFA_Alternation(stateType_t* indexState, expressionAlternation_t* expression)
{
    ccLogTrace();
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_concatenation = NULL;
    nfa_t* nfa_alternation = NULL;

    switch(expression->type){
    case typeAlternation_AlternationConcatenation:
        nfa_concatenation = buildNFA_Concatenation(indexState, expression->concatenation);
        nfa_alternation = buildNFA_Alternation(indexState, expression->alternation);
        // opAlternation(indexState, nfa_concatenation, nfa_alternation);
        // nfa_result = nfa_concatenation;
        opAlternation(indexState, nfa_alternation, nfa_concatenation);
        nfa_result = nfa_alternation;
        break;
    case typeAlternation_Concatenation:
        nfa_result = buildNFA_Concatenation(indexState, expression->concatenation);
        break;
    case typeAlternation_Empty:
        break;
    default:
        ccLogError("Default case should be unreachable!");
        break;
    }

    return nfa_result;
}

nfa_t* buildNFA_RegularExpression(stateType_t* indexState, expressionRegularExpression_t* expression)
{
    ccLogTrace();
    nfa_t* nfa = NULL;

    switch(expression->type){
    case typeRegularExpression_RegularExpression:
        nfa = buildNFA_Alternation(indexState, expression->alternation);
        break;
    case typeRegularExpression_Empty:
    case typeRegularExpression_EOF:
        break;
    default:
        ccLogError("Default case should be unreachable!");
        break;        
    }

    return nfa;
}

/* Ideally we should interleave the NFA building code with the actual 
 * parsing code. I don't see any reason to do it after parsing other 
 * than to delay a refactoring of the code */
nfa_t* buildNFA(expressionRegularExpression_t* expression)
{
    stateType_t indexState = 0;
    nfa_t* nfa = NULL;

    nfa = buildNFA_RegularExpression(&indexState, expression);
    // if(ccLog_isLogLevelActive(ccLogLevels_Debug))
    //     dbg_printNFA(nfa);
    fixupStates(nfa);
    // if(ccLog_isLogLevelActive(ccLogLevels_Debug))
    //     dbg_printNFAStates(nfa);

    return nfa;
}

nfa_transition_t* nfa_transition_ctor(stateType_t fromState, stateType_t toState, char isComplement, char isEpsilon, char symbol)
{
    nfa_transition_t* transition = malloc(sizeof(nfa_transition_t));

    transition->fromState = fromState;
    transition->toState = toState;
    transition->isComplement = isComplement;
    transition->isEpsilon = isEpsilon;
    transition->symbol = symbol;

    return transition;
}

nfa_t* nfa_ctor(void)
{
    nfa_t* nfa = NULL;

    expectExit(nfa, malloc(sizeof(nfa_t)), != NULL);
    nfa->start = NULL;
    nfa->accept = NULL;
    nfa->transitions = ccList_ctor();
    nfa->states = NULL;

    return nfa;
}

void nfa_dtor(nfa_t* data)
{
    ccList_dtor(data->transitions);
    ccDynamicArray_dtor(data->states);
    free(data);
}

void nfa_shallowDtor(nfa_t* data){
    free(data->transitions);
    free(data);
}

static void dbg_printTransition(nfa_transition_t* transition)
{
    ccLogDebug(
        "\nfromState: \t%ld\n"
        "toState: \t%ld\n"
        "isCompl: \t%s\n"
        "isEpsil: \t%s\n"
        "symbol: \t%c (0x%x)\n",
        transition->fromState,
        transition->toState,
        (int)transition->isComplement ? "True" : "False",
        (int)transition->isEpsilon ? "True" : "False",
        transition->symbol,
        transition->symbol
    );
}

/* This functions sorts ll nodes by fromState and populates the states data structure */
void fixupStates(nfa_t* nfa)
{
    ccListNode_t* node = NULL;
    ccListNode_t* representative = NULL;
    nfa_transition_t* transition = NULL;
    size_t transitions = nfa->transitions->size;

    /* we populate the state array with linked lists, effectively sorting the transitions by fromState */
    nfa->states = ccDynamicArray_ctor(sizeof(ccListNode_t*), true);
    for(size_t i = 0; i < transitions; ++i){
        node = nfa->transitions->head;
        ccList_unlink(nfa->transitions, node);
        transition = (nfa_transition_t*)node->data;
        representative = *(ccListNode_t**)ccDynamicArray_get(nfa->states, transition->fromState);
        if(representative == NULL){
            ccDynamicArray_set(nfa->states, transition->fromState, &node);
        }else{
            representative->previous = node;
            node->next = representative;
            ccDynamicArray_set(nfa->states, transition->fromState, &node);
        }
    }

    /* TODO: do we even need to relink the whole list ? */
}

void dbg_printNFA(nfa_t* nfa)
{
    ccLogDebug("\n\n\n\nSTART PRINT NFA");
    ccLogDebug("start:");
    dbg_printTransition((nfa_transition_t*)nfa->start->data);
    ccLogDebug("accept:");
    dbg_printTransition((nfa_transition_t*)nfa->accept->data);
    for(size_t i = 0; i < nfa->transitions->size; ++i){
        dbg_printTransition((nfa_transition_t*)ccList_itemAt(nfa->transitions, i));
    }
    ccLogDebug("END PRINT NFA\n\n\n\n");
}

void dbg_printNFAStates(nfa_t* nfa)
{
    int doPrint = 0;
    ccListNode_t* node = NULL;

    ccLogDebug("start:");
    dbg_printTransition((nfa_transition_t*)nfa->start->data);
    ccLogDebug("accept:");
    dbg_printTransition((nfa_transition_t*)nfa->accept->data);

    if(nfa->states == NULL)
        return;

    for(size_t i = 0; i < nfa->states->size; ++i){
        node = *(ccListNode_t**)ccDynamicArray_get(nfa->states, i);
        doPrint = 0;
        if(node != NULL)
            doPrint = 1;
        if(doPrint == 1)
            ccLogDebug("LINKAGE START");
        while(node != NULL){
            dbg_printTransition((nfa_transition_t*)node->data);
            node = node->next;
        }
        if(doPrint == 1)
            ccLogDebug("LINKAGE END\n");
    }
}