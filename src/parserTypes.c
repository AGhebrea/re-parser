
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "cclog.h"
#include "cclog_macros.h"
#include <parserTypes.h>

void dbg_printCaptureGroup(expressionCaptureGroup_t* expression, size_t level);
void dbg_printTerm(expressionTerm_t* expression, size_t level);
void dbg_printComplement(expressionComplement_t* expression, size_t level);
void dbg_printClosure(expressionClosure_t* expression, size_t level);
void dbg_printConcatenation(expressionConcatenation_t* expression, size_t level);
void dbg_printAlternation(expressionAlternation_t* expression, size_t level);
void dbg_printRegularExpression(expressionRegularExpression_t* expression, size_t level);
void dbg_printRE(expressionRegularExpression_t* expression);

expressionRegularExpression_t* ctor_expressionRegularExpression()
{
    expressionRegularExpression_t* expression = malloc(sizeof(expressionRegularExpression_t));
    expression->type = typeRegularExpression_NULL;
    return expression;
}

expressionAlternation_t* ctor_expressionAlternation()
{
    expressionAlternation_t* expression = malloc(sizeof(expressionAlternation_t));
    expression->type = typeAlternation_NULL;
    return expression;
}

expressionConcatenation_t* ctor_expressionConcatenation()
{
    expressionConcatenation_t* expression = malloc(sizeof(expressionConcatenation_t));
    expression->type = typeConcatenation_NULL;
    return expression;
}

expressionClosure_t* ctor_expressionClosure()
{
    expressionClosure_t* expression = malloc(sizeof(expressionClosure_t));
    expression->type = typeClosure_NULL;
    return expression;
}

expressionComplement_t* ctor_expressionComplement()
{
    expressionComplement_t* expression = malloc(sizeof(expressionComplement_t));
    expression->type = typeComplement_NULL;
    return expression;
}

expressionTerm_t* ctor_expressionTerm()
{
    expressionTerm_t* expression = malloc(sizeof(expressionTerm_t));
    expression->type = typeTerm_NULL;
    return expression;
}

expressionCaptureGroup_t* ctor_expressionCaptureGroup()
{
    expressionCaptureGroup_t* expression = malloc(sizeof(expressionCaptureGroup_t));
    expression->type = typeCaptureGroup_NULL;
    return expression;
}

void dtor_expressionRegularExpression(expressionRegularExpression_t* data)
{
    if(data == NULL)
        return;

    switch(data->type){
    case typeRegularExpression_RegularExpression:
        dtor_expressionAlternation(data->alternation);
        break;
    case typeRegularExpression_NULL:
    case typeRegularExpression_Empty:
    case typeRegularExpression_EOF:
        break;
    default:
        ccLogError("Unknown Data Type %d", data->type);
        exit(1);
    }

    free(data);
}

void dtor_expressionAlternation(expressionAlternation_t* data)
{
    if(data == NULL)
        return;
    
    switch(data->type){
    case typeAlternation_Concatenation:
        dtor_expressionConcatenation(data->concatenation);
        break;
    case typeAlternation_AlternationConcatenation:
        dtor_expressionConcatenation(data->concatenation);
        dtor_expressionAlternation(data->alternation);
        break;
    case typeAlternation_NULL:
    case typeAlternation_Empty:
        break;
    default:
        ccLogError("Unknown Data Type %d", data->type);
        exit(1);
    }

    free(data);
}

void dtor_expressionConcatenation(expressionConcatenation_t* data)
{
    if(data == NULL)
        return;
    
    switch(data->type){
    case typeConcatenation_Closure:
    case typeConcatenation_Concatentation:
        dtor_expressionClosure(data->closure);
        dtor_expressionConcatenation(data->concatenation);
        break;
    case typeConcatenation_Empty:
    case typeConcatenation_NULL:
        break;
    default:
        ccLogError("Unknown Data Type %d", data->type);
        exit(1);
    }

    free(data);
}

void dtor_expressionClosure(expressionClosure_t* data)
{
    if(data == NULL)
        return;
    
    switch(data->type){
    case typeClosure_Kleene:
    case typeClosure_Positive:
    case typeClosure_Explicit:
    case typeClosure_Complement:
        dtor_expressionComplement(data->complement);
        break;
    case typeClosure_NULL:
        break;
    default:
        ccLogError("Unknown Data Type %d", data->type);
        exit(1);
    }

    free(data);
}

void dtor_expressionComplement(expressionComplement_t* data)
{
    if(data == NULL)
        return;
    
    switch(data->type){
    case typeComplement_Term:
    case typeComplement_Complement:
        dtor_expressionTerm(data->term);
        break;
    case typeTerm_NULL:
        break;
    default:
        ccLogError("Unknown Data Type %d", data->type);
        exit(1);
    }

    free(data);
}

void dtor_expressionTerm(expressionTerm_t* data)
{
    if(data == NULL)
        return;

    switch(data->type){
    case typeTerm_RegularExpression:
        dtor_expressionRegularExpression(data->value.regularExpression);
        break;
    case typeTerm_CaptureGroup:
        dtor_expressionCaptureGroup(data->value.captureGroup);
        break;
    case typeTerm_Character:
    case typeTerm_Epsilon:
        break;
    case typeTerm_NULL:
        break;
    default:
        ccLogError("Unknown Data Type %d", data->type);
        exit(1);
    }

    free(data);
}

void dtor_expressionCaptureGroup(expressionCaptureGroup_t* data)
{
    free(data);
}

void dbg_printCaptureGroup(expressionCaptureGroup_t* expression, size_t level)
{
    char* preamble = malloc(sizeof(char) * (level * 2 + 1));
    memset(preamble, ' ', level * 2);
    preamble[level*2] = '\0';
    switch(expression->type){
    case typeCaptureGroup_NULL:
        ccLogWarn("\n%stypeCaptureGroup_NULL", preamble);
        break;
    case typeCaptureGroup_CaptureGroup:
        printf("%s[%c...%c]\n", preamble, expression->leftCharacter, expression->rightCharacter);
        break;
    default:
        ccLogError("???");
        exit(1);
    }
    free(preamble);
}

void dbg_printTerm(expressionTerm_t* expression, size_t level)
{
    char* preamble = malloc(sizeof(char) * (level * 2 + 1));
    memset(preamble, ' ', level * 2);
    preamble[level*2] = '\0';
    switch(expression->type){
    case typeTerm_RegularExpression:
        dbg_printRegularExpression(expression->value.regularExpression, level);
        break;
    case typeTerm_CaptureGroup:
        dbg_printCaptureGroup(expression->value.captureGroup, level);
        break;
    case typeTerm_Character:
        if(isspace(expression->value.character)){
            switch(expression->value.character){
            case ' ':
                printf("%s<SPACE>\n", preamble);
                break;
            case '\f':
                printf("%s\\f\n", preamble);
                break;
            case '\n':
                printf("%s\\n\n", preamble);
                break;
            case '\r':
                printf("%s\\r\n", preamble);
                break;
            case '\t':
                printf("%s\\t\n", preamble);
                break;
            case '\v':
                printf("%s\\v\n", preamble);
                break;
            default: 
                ccLogError("???");
                exit(1);
            }
        }else{
            printf("%s%c\n", preamble, expression->value.character);
        }
        break;
    case typeTerm_Epsilon:
        printf("%sEpsilon\n", preamble);
        break;
    default:
        ccLogError("???");
        exit(1);
    }
    free(preamble);
}

void dbg_printComplement(expressionComplement_t* expression, size_t level)
{
    char* preamble = malloc(sizeof(char) * (level * 2 + 1));
    memset(preamble, ' ', level * 2);
    preamble[level*2] = '\0';
    switch(expression->type){
    case typeComplement_Term:
        dbg_printTerm(expression->term, level);
        break;
    case typeComplement_Complement:
        printf("%sComplement\n", preamble);
        dbg_printTerm(expression->term, level + 1);
        break;
    default:
        ccLogError("???");
        exit(1);
    }
    free(preamble);
}

void dbg_printClosure(expressionClosure_t* expression, size_t level)
{
    char* preamble = malloc(sizeof(char) * (level * 2 + 1));
    memset(preamble, ' ', level * 2);
    preamble[level*2] = '\0';
    switch(expression->type){
    case typeClosure_NULL:
        ccLogWarn("\n%stypeClosure_NULL", preamble);
        break;
    case typeClosure_Kleene:
        printf("%sKleene\n", preamble);
        dbg_printComplement(expression->complement, level + 1);
        break;
    case typeClosure_Positive:
        printf("%sPositive\n", preamble);
        dbg_printComplement(expression->complement, level + 1);
        break;
    case typeClosure_Explicit:
        printf("%sExplicit {%ld}\n", preamble, expression->repetitionNumber);
        dbg_printComplement(expression->complement, level + 1);
        break;
    case typeClosure_Complement:
        dbg_printComplement(expression->complement, level);
        break;
    default:
        ccLogError("???");
        exit(1);
    }
    free(preamble);
}

void dbg_printConcatenation(expressionConcatenation_t* expression, size_t level)
{
    char* preamble = malloc(sizeof(char) * (level * 2 + 1));
    memset(preamble, ' ', level * 2);
    preamble[level*2] = '\0';
    switch(expression->type){
    case typeConcatenation_NULL:
        ccLogWarn("\n%stypeConcatenation_NULL", preamble);
        break;
    case typeConcatenation_Empty:
        break;
    case typeConcatenation_Concatentation:
        printf("%sConcatenation\n", preamble);
        dbg_printClosure(expression->closure, level + 1);
        dbg_printConcatenation(expression->concatenation, level + 1);
        break;
    case typeConcatenation_Closure:
        dbg_printClosure(expression->closure, level);
        break;
    default:
        ccLogError("???");
        exit(1);
    }
    free(preamble);
}

void dbg_printAlternation(expressionAlternation_t* expression, size_t level)
{
    char* preamble = malloc(sizeof(char) * (level * 2 + 1));
    memset(preamble, ' ', level * 2);
    preamble[level*2] = '\0';
    switch(expression->type){
    case typeAlternation_NULL:
        ccLogWarn("\n%stypeAlternation_NULL", preamble);
        break;
    case typeAlternation_Empty:
        break;
    case typeAlternation_Concatenation:
        dbg_printConcatenation(expression->concatenation, level);
        break;
    case typeAlternation_AlternationConcatenation:
        printf("%sAlternation\n", preamble);
        dbg_printConcatenation(expression->concatenation, level + 1);
        dbg_printAlternation(expression->alternation, level + 1);
        break;
    default:
        ccLogError("???");
        exit(1);
    }
    free(preamble);
}

void dbg_printRegularExpression(expressionRegularExpression_t* expression, size_t level)
{
    char* preamble = malloc(sizeof(char) * (level * 2 + 1));
    memset(preamble, ' ', level * 2);
    preamble[level*2] = '\0';
    switch(expression->type){
    case typeRegularExpression_NULL:
        ccLogWarn("\n%stypeRegularExpression_NULL", preamble);
        break;
    case typeRegularExpression_Empty:
        break;
    case typeRegularExpression_EOF:
        printf("%stypeRegularExpression_EOF", preamble);
        break;
    case typeRegularExpression_RegularExpression:
        dbg_printAlternation(expression->alternation, level);
        break;
    default:
        ccLogError("???");
        exit(1);
    }
    free(preamble);
}

void dbg_printRE(expressionRegularExpression_t* expression)
{
    if(!ccLog_isLogLevelActive(ccLogLevels_Debug))
        return;
    
    printf("\n\nRegularExpression\n");
    dbg_printRegularExpression(expression, 0);
}