#include "ccList.h"
#include "cclog_macros.h"
#include "parserTypes.h"
#include <nfa.h>
#include <stdint.h>
#include <stdlib.h>
#include <cclog.h>

/* NOTE: If adding identic states becomes an issue we can add a rb-tree to nfa_t to store
 * info about transitions already present in the sets. */
/* TODO: interleave the NFA build code with the parsing code. */

/* hopefully lessons learned */
/* TODO: if you think about it, we only mostly care about START and ACC of each set, 
 * we could try to use a single set and return some sort of control data structure to be able to
 * only use a single set. What did we learn ? to keep it simpler, i wasted a lot of time on pondering 
 * how to do this and it could've been figured out faster by asking ourselves "What do we really want to do ?"
 * as always. */
/* NOTE: we could've done a bit better on AST building, now that I look at it a bit, there are 
 * inconsistencies. */

/* NOTE: These should always store result into argument 'a' */
void opComplement(nfa_t* a);
void opConcatenation(nfa_t* a, nfa_t* b);
void opAlternation(nfa_t* a, nfa_t* b);
void opKleene(nfa_t* a);
void opPositiveClosure(nfa_t* a);
void opExplicitClosure(nfa_t* a, size_t times);

nfa_t* buildNFA_Epsilon(int* groupsState);
nfa_t* buildNFA_Character(int* groupsState, char expression);
nfa_t* buildNFA_CaptureGroup(int* groupsState, expressionCaptureGroup_t* expression);
nfa_t* buildNFA_Term(int* groupsState, expressionTerm_t* expression);
nfa_t* buildNFA_Complement(int* groupsState, expressionComplement_t* expression);
nfa_t* buildNFA_Closure(int* groupsState, expressionClosure_t* expression);
nfa_t* buildNFA_Concatenation(int* groupsState, expressionConcatenation_t* expression);
nfa_t* buildNFA_Alternation(int* groupsState, expressionAlternation_t* expression);
nfa_t* buildNFA_RegularExpression(int* groupsState, expressionRegularExpression_t* expression);

/* To complement a regular expression (RE), for every non-epsilon transition in the original RE, 
 * we add a new transition from the same starting state to an auxiliary state. This new transition 
 * is made on the complement of the original symbol. From each of these auxiliary states, we then 
 * create an epsilon transition to a new acknowledgment (ACK) state. Essentially, this process 
 * marks all states from the original RE, except its original ACK state, as valid acknowledgment 
 * states in the complemented RE. */
void opComplement(nfa_t* a)
{
    ccListNode_t* ackTransition = NULL;
    transition_t* transition = NULL;
    short index = a->states->size;
    short ackIndex = index;

    ++index;
    /* TODO: what if there are only epsilon transitions ? do we allow that ? 
     * sort that out */
    for(size_t i = 0; i < a->states->size; ++i){
        transition = (transition_t*)ccList_itemAt(a->states, i);
        if(transition->isEpsilon != 0)
            continue;
        ccList_append(a->states, ccListNode_ctor(transition_ctor(transition->group, 
            transition->fromState, index, 1, 0, transition->symbol), free));
        ackTransition = ccListNode_ctor(transition_ctor(transition->group, index, 
            ackIndex, 0, 1, '?'), free);
        ccList_append(a->states, ackTransition);   
        index++;
    }

    a->accept = ackTransition;
}

void opConcatenation(nfa_t* a, nfa_t* b)
{
    // TODO
}

void opAlternation(nfa_t* a, nfa_t* b)
{
    // TODO
}

void opKleene(nfa_t* a)
{
    // TODO
}

void opPositiveClosure(nfa_t* a)
{
    // TODO
}

void opExplicitClosure(nfa_t* a, size_t times)
{
    // TODO
}

nfa_t* buildNFA_Epsilon(int* groupsState)
{
    nfa_t* nfa_result = nfa_ctor();
    ccListNode_t* node;
    
    node = ccListNode_ctor(transition_ctor(*groupsState, 0, 1, 0, 1, '?'), free);
    ccList_append(nfa_result->states, node);
    nfa_result->start = node;
    nfa_result->accept = node;
    *groupsState += 1;

    return nfa_result;
}

nfa_t* buildNFA_Character(int* groupsState, char expression)
{
    nfa_t* nfa_result =  nfa_ctor();
    ccListNode_t* node;
    
    node = ccListNode_ctor(transition_ctor(*groupsState, 0, 1, 0, 0, expression), free);
    ccList_append(nfa_result->states, node);
    nfa_result->start = node;
    nfa_result->accept = node;
    *groupsState += 1;

    return nfa_result;
}

nfa_t* buildNFA_CaptureGroup(int* groupsState, expressionCaptureGroup_t* expression)
{
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_aux = NULL;

    nfa_result = buildNFA_Character(groupsState, (char)expression->leftCharacter);
    for(size_t i = expression->leftCharacter + 1; i < expression->rightCharacter; ++i){
        nfa_aux = buildNFA_Character(groupsState, (char)i);
        opAlternation(nfa_result, nfa_aux);
    }

    return nfa_result;
}

nfa_t* buildNFA_Term(int* groupsState, expressionTerm_t* expression)
{
    nfa_t* nfa_result = NULL;

    switch(expression->type){
    case typeTerm_RegularExpression:
        nfa_result = buildNFA_RegularExpression(groupsState, expression->value.regularExpression);
        break;
    case typeTerm_CaptureGroup:
        nfa_result = buildNFA_CaptureGroup(groupsState, expression->value.captureGroup);
        break;
    case typeTerm_Character:
        nfa_result = buildNFA_Character(groupsState, expression->value.character);
        break;
    case typeTerm_Epsilon:
        nfa_result = buildNFA_Epsilon(groupsState);
        break;
    default:
        ccLogError("Default case should be unreachable!");
        break;
    }

    return nfa_result;
}

nfa_t* buildNFA_Complement(int* groupsState, expressionComplement_t* expression)
{
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_term = NULL;

    nfa_term = buildNFA_Term(groupsState, expression->term);
    switch(expression->type){
    case typeComplement_Complement:
        opComplement(nfa_term);
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

nfa_t* buildNFA_Closure(int* groupsState, expressionClosure_t* expression)
{
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_complement = NULL;

    nfa_complement = buildNFA_Complement(groupsState, expression->complement);
    switch(expression->type){
    case typeClosure_Kleene:
        opKleene(nfa_complement);
        break;
    case typeClosure_Positive:
        opPositiveClosure(nfa_complement);
        break;
    case typeClosure_Explicit:
        opExplicitClosure(nfa_complement, expression->repetitionNumber);
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

nfa_t* buildNFA_Concatenation(int* groupsState, expressionConcatenation_t* expression)
{
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_concatenation = NULL;
    nfa_t* nfa_closure = NULL;

    switch(expression->type){
    case typeConcatenation_ConcatentationClosure:
        nfa_concatenation = buildNFA_Concatenation(groupsState, expression->concatenation);
        nfa_closure = buildNFA_Closure(groupsState, expression->closure);
        opConcatenation(nfa_concatenation, nfa_closure);
        nfa_result = nfa_concatenation;
        break;
    case typeConcatenation_Closure:
        nfa_result = buildNFA_Closure(groupsState, expression->closure);
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

nfa_t* buildNFA_Alternation(int* groupsState, expressionAlternation_t* expression)
{
    nfa_t* nfa_result = NULL;
    nfa_t* nfa_concatenation = NULL;
    nfa_t* nfa_alternation = NULL;

    switch(expression->type){
    case typeAlternation_AlternationConcatenation:
        nfa_concatenation = buildNFA_Concatenation(groupsState, expression->concatenation);
        nfa_alternation = buildNFA_Alternation(groupsState, expression->alternation);
        opAlternation(nfa_concatenation, nfa_alternation);
        nfa_result = nfa_concatenation;
        break;
    case typeAlternation_Concatenation:
        nfa_result = buildNFA_Concatenation(groupsState, expression->concatenation);
        break;
    case typeAlternation_Empty:
        break;
    default:
        ccLogError("Default case should be unreachable!");
        break;
    }

    return nfa_result;
}

nfa_t* buildNFA_RegularExpression(int* groupsState, expressionRegularExpression_t* expression)
{
    nfa_t* nfa = NULL;

    switch(expression->type){
    case typeRegularExpression_RegularExpression:
        nfa = buildNFA_Alternation(groupsState, expression->alternation);
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
void buildNFA(expressionRegularExpression_t* expression)
{
    int groupState = 0;
    nfa_t* nfa = NULL;

    nfa = buildNFA_RegularExpression(&groupState, expression);

    (void)nfa;

    return;
}

transition_t* transition_ctor(int group, int fromState, int toState, char isComplement, char isEpsilon, char symbol)
{
    transition_t* transition = malloc(sizeof(transition_t));

    transition->group = group;
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
    nfa->groupSize = 0;

    return nfa;
}

void nfa_dtor(nfa_t* data)
{
    ccLogNotImplemented;
}