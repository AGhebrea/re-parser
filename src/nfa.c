#include "ccList.h"
#include "cclog_macros.h"
#include "parserTypes.h"
#include <nfa.h>
#include <stdint.h>
#include <stdlib.h>
#include <cclog.h>

// #define TRACEDEBUG

/* NOTE: idk how to formally define the id generation mechanism. It's auto incrementing
 * and the criterion is that index always is greater than all states ever created */

/* TODO: fix memory leaks */

/* NOTE: If adding identic states becomes an issue we can add a rb-tree to nfa_t to store
 * info about transitions already present in the sets. */
/* TODO: interleave the NFA build code with the parsing code. */
/* NOTE: A general thing that i keep thinking about is if there are any ways to represent 
 * more complex data structures in a flatter manner and not have to dereference more than 
 * 1-2 times to get to values. Also, isn't there a simpler way to shuffle data around like 
 * this ? */

/* hopefully lessons learned */
/* TODO: if you think about it, we only mostly care about START and ACC of each set, 
 * we could try to use a single set and return some sort of control data structure to be able to
 * only use a single set. What did we learn ? to keep it simpler, i wasted a lot of time on pondering 
 * how to do this and it could've been figured out faster by asking ourselves "What do we really want to do ?"
 * as always. */
/* NOTE: we could've done a bit better on AST building, now that I look at it a bit, there are 
 * inconsistencies. */

/* NOTE: These should always store result into argument 'a' */
void opComplement(int* indexState, nfa_t* a);
void opConcatenation(int* indexState, nfa_t* a, nfa_t* b);
void opAlternation(int* indexState, nfa_t* a, nfa_t* b);
void opKleene(int* indexState, nfa_t* a);
void opPositiveClosure(int* indexState, nfa_t* a);
void opExplicitClosure(int* indexState, nfa_t* a, size_t times);

nfa_t* buildNFA_Epsilon(int* indexState);
nfa_t* buildNFA_Character(int* indexState, char expression);
nfa_t* buildNFA_CaptureGroup(int* indexState, expressionCaptureGroup_t* expression);
nfa_t* buildNFA_Term(int* indexState, expressionTerm_t* expression);
nfa_t* buildNFA_Complement(int* indexState, expressionComplement_t* expression);
nfa_t* buildNFA_Closure(int* indexState, expressionClosure_t* expression);
nfa_t* buildNFA_Concatenation(int* indexState, expressionConcatenation_t* expression);
nfa_t* buildNFA_Alternation(int* indexState, expressionAlternation_t* expression);
nfa_t* buildNFA_RegularExpression(int* indexState, expressionRegularExpression_t* expression);

void dbg_printNFA(nfa_t* nfa);

nfa_t* copyNFA(int* indexState, nfa_t* nfa){
    ccListNode_t* node = NULL;
    transition_t* transition = NULL;
    transition_t* start = nfa->start->data;
    transition_t* accept = nfa->accept->data;
    nfa_t* newNFA = nfa_ctor();
    ccList_t* newList = ccList_ctor();
    ccList_t* list = nfa->states;
    int index = *indexState;

    for(size_t i = 0; i < list->size; ++i){
        transition = (transition_t*)ccList_itemAt(list, i);
        node = ccListNode_ctor(transition_ctor(
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
void opComplement(int* indexState, nfa_t* a)
{
    bool appendedAck = false;
    ccListNode_t* ackTransition = NULL;
    transition_t* transition = NULL;
    size_t size = a->states->size;
    short ackIndex = *indexState;
    *indexState += 1;

    /* TODO: what if there are only epsilon transitions ? do we allow that ? 
     * sort that out */
    /* TODO: when we unlink, we might drop an entire graph, we should look into the effects of that */
    for(size_t i = 0; i < size; ++i){
        transition = (transition_t*)ccList_itemAt(a->states, i);
        if(transition->isEpsilon != 0)
            continue;
        /* TODO: see if you can do something about the fact that it takes a lot of boilerplate to add a transition to the list 
         * we could make specialized functions, e.g append_epsilon, append_symbol, etc. or we can just leave it like this */
        ccList_append(a->states, ccListNode_ctor(transition_ctor( 
            transition->fromState, (*indexState), 1, 0, transition->symbol), free));
        if(appendedAck == false){
            ackTransition = ccListNode_ctor(transition_ctor( (*indexState), 
                ackIndex, 0, 1, '?'), free);
            ccList_append(a->states, ackTransition);
            appendedAck = true;
        }
    }

    *indexState += 1;
    ccList_unlink(a->states, a->accept);
    ccListNode_dtor(a->accept);
    a->accept = ackTransition;
}


void opConcatenation(int* indexState, nfa_t* a, nfa_t* b)
{
    transition_t* transition_a = NULL;
    transition_t* transition_b = NULL;

    (void)indexState;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Before");
    ccLogDebug("A");
    dbg_printNFA(a);
    ccLogDebug("B");
    dbg_printNFA(b);
#endif // TRACEDEBUG

    ccList_join(a->states, b->states);

    transition_a = (transition_t*)a->accept->data;
    transition_b = (transition_t*)b->start->data;

    ccList_append(a->states, ccListNode_ctor(transition_ctor( 
        transition_a->toState, transition_b->fromState, 0, 1, '?'), free));
    a->accept = b->accept;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Result");
    dbg_printNFA(a);
#endif // TRACEDEBUG
}

void opAlternation(int* indexState, nfa_t* a, nfa_t* b)
{
    transition_t* transition = NULL;
    ccListNode_t* node = NULL;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Before");
    ccLogDebug("A");
    dbg_printNFA(a);
    ccLogDebug("B");
    dbg_printNFA(b);
#endif // TRACEDEBUG

    ccList_join(a->states, b->states);
    transition = (transition_t*)a->start->data;
    node = ccListNode_ctor(transition_ctor( *indexState, transition->fromState, 0, 1, '?'), free);
    ccList_append(a->states, node);
    transition = (transition_t*)b->start->data;
    node = ccListNode_ctor(transition_ctor( *indexState, transition->fromState, 0, 1, '?'), free);
    ccList_append(a->states, node);
    a->start = node;
    *indexState += 1;

    transition = (transition_t*)a->accept->data;
    node = ccListNode_ctor(transition_ctor( transition->toState, *indexState, 0, 1, '?'), free);
    ccList_append(a->states, node);
    transition = (transition_t*)b->accept->data;
    node = ccListNode_ctor(transition_ctor( transition->toState, *indexState, 0, 1, '?'), free);
    ccList_append(a->states, node);
    a->accept = node;
    *indexState += 1;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Result");
    dbg_printNFA(a);
#endif // TRACEDEBUG
}

void opKleene(int* indexState, nfa_t* a)
{
    transition_t* ack = NULL;
    transition_t* start = NULL;
    ccListNode_t* node = NULL;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Before");
    ccLogDebug("A");
    dbg_printNFA(a);
#endif // TRACEDEBUG

    ack = (transition_t*)a->accept->data;
    start = (transition_t*)a->start->data;
    node = ccListNode_ctor(transition_ctor(ack->toState, *indexState, 0, 1, '?'), free);
    ccList_append(a->states, node);
    a->accept = node;
    ccList_append(a->states, ccListNode_ctor(transition_ctor(start->fromState, *indexState, 0, 1, '?'), free));
    ccList_append(a->states, ccListNode_ctor(transition_ctor(ack->toState, start->fromState, 0, 1, '?'), free));

    *indexState += 1;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Result");
    dbg_printNFA(a);
#endif // TRACEDEBUG
}

void opPositiveClosure(int* indexState, nfa_t* a)
{
    transition_t* ack = NULL;
    transition_t* start = NULL;
    ccListNode_t* node = NULL;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Before");
    ccLogDebug("A");
    dbg_printNFA(a);
#endif // TRACEDEBUG

    ack = (transition_t*)a->accept->data;
    start = (transition_t*)a->start->data;
    node = ccListNode_ctor(transition_ctor(ack->toState, *indexState, 0, 1, '?'), free);
    ccList_append(a->states, node);
    a->accept = node;
    ccList_append(a->states, ccListNode_ctor(transition_ctor(ack->toState, start->fromState, 0, 1, '?'), free));

    *indexState += 1;

#ifdef TRACEDEBUG
    ccLogTrace();
    ccLogDebug("Result");
    dbg_printNFA(a);
#endif // TRACEDEBUG
}

void opExplicitClosure(int* indexState, nfa_t* a, size_t times)
{
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

nfa_t* buildNFA_Epsilon(int* indexState)
{
    nfa_t* nfa_result = nfa_ctor();
    ccListNode_t* node;
    
    node = ccListNode_ctor(transition_ctor(*indexState, *indexState + 1, 0, 1, '?'), free);
    ccList_append(nfa_result->states, node);
    nfa_result->start = node;
    nfa_result->accept = node;
    *indexState += 2;

    return nfa_result;
}

nfa_t* buildNFA_Character(int* indexState, char expression)
{
    nfa_t* nfa_result =  nfa_ctor();
    ccListNode_t* node;
    
    node = ccListNode_ctor(transition_ctor(*indexState, *indexState + 1, 0, 0, expression), free);
    ccList_append(nfa_result->states, node);
    nfa_result->start = node;
    nfa_result->accept = node;
    *indexState += 2;

    return nfa_result;
}

nfa_t* buildNFA_CaptureGroup(int* indexState, expressionCaptureGroup_t* expression)
{
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_aux = NULL;

    nfa_result = buildNFA_Character(indexState, (char)expression->leftCharacter);
    for(size_t i = expression->leftCharacter + 1; i < expression->rightCharacter; ++i){
        nfa_aux = buildNFA_Character(indexState, (char)i);
        opAlternation(indexState, nfa_result, nfa_aux);
    }

    return nfa_result;
}

nfa_t* buildNFA_Term(int* indexState, expressionTerm_t* expression)
{
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

nfa_t* buildNFA_Complement(int* indexState, expressionComplement_t* expression)
{
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

nfa_t* buildNFA_Closure(int* indexState, expressionClosure_t* expression)
{
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

nfa_t* buildNFA_Concatenation(int* indexState, expressionConcatenation_t* expression)
{
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

nfa_t* buildNFA_Alternation(int* indexState, expressionAlternation_t* expression)
{
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

nfa_t* buildNFA_RegularExpression(int* indexState, expressionRegularExpression_t* expression)
{
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
    int indexState = 0;
    nfa_t* nfa = NULL;

    nfa = buildNFA_RegularExpression(&indexState, expression);

    return nfa;
}

transition_t* transition_ctor(int fromState, int toState, char isComplement, char isEpsilon, char symbol)
{
    transition_t* transition = malloc(sizeof(transition_t));

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
    nfa->states = ccList_ctor();

    return nfa;
}

void nfa_dtor(nfa_t* data)
{
    ccLogNotImplemented;
}

void dbg_printTransition(transition_t* transition)
{
    ccLogDebug(
        "\nfromState: \t%d\n"
        "toState: \t%d\n"
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

void dbg_printNFA(nfa_t* nfa)
{
    ccLogDebug("\n\n\n\nSTART PRINT NFA");
    ccLogDebug("start:");
    dbg_printTransition((transition_t*)nfa->start->data);
    ccLogDebug("accept:");
    dbg_printTransition((transition_t*)nfa->accept->data);
    for(size_t i = 0; i < nfa->states->size; ++i){
        dbg_printTransition((transition_t*)ccList_itemAt(nfa->states, i));
    }
    ccLogDebug("END PRINT NFA\n\n\n\n");
}