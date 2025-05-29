#include <ctype.h>
#include <stdlib.h>
#include <parser.h>
#include <parserTypes.h>
#include <cclog_macros.h>
#include <stdbool.h>
#include <lexer.h>
#include <threads.h>
#include <nfa.h>

#define DO_NOTHING (void)0

extern lexer_t lexer;

// todo: create error signalling mechanism
// todo: you could cache the token that you peeked and return that if you 
// peek multiple times
// todo: we cannot easily parse +- chars, and etc. We could easily do it if we had 
// stuff like major:minor token types. 

char parseCharacter();
size_t parseUInt();
expressionCaptureGroup_t* parseCaptureGroup();
expressionTerm_t* parseTerm();
expressionComplement_t* parseComplement();
expressionClosure_t* parseClosure();
expressionConcatenation_t* parseConcatenation();
expressionAlternation_t* parseAlternationPrime();
expressionAlternation_t* parseAlternation();
expressionRegularExpression_t* parseRegularExpression();
expressionRegularExpression_t* parseRegularExpressionPrime();

void printToken(token_t* token)
{
    ccLogInfo("(%s)[%s]", lexer_getTokenString(token->major), token->value);
}

void printError(char* errorMessage)
{
    fprintf(stderr, "[%s:%ld] %s\n", lexer.fileName, lexer.charCount, errorMessage);
}

size_t parseUInt()
{
    token_t token;
    size_t value = 0;
    
    lexer_nextToken(&token);
    if(isdigit(token.value)){
        do{
            value = value * 10;
            value += (token.value) - '0';
            lexer_nextToken(&token);
        }while(isdigit(token.value));
    }else{
        goto fail;
    }

    return value;

fail:
    // todo: report error nicely
    ccLogError("Failed to parse UINT");
    exit(1);
}

char parseCharacter()
{
    char value = '\0';
    token_t token;

    lexer_nextToken(&token);
    if(token.major == tokenMajor_Character)
        value = token.value;

    return value;
}

expressionCaptureGroup_t* parseCaptureGroup()
{
    expressionCaptureGroup_t* expression = NULL;
    token_t token;
    expression = ctor_expressionCaptureGroup();
    expression->type = typeCaptureGroup_CaptureGroup;

    lexer_nextToken(&token);
    if(token.major != tokenMajor_OpenBracket){
        printError("Expected OpenBracket");
        goto fail;
    }

    expression->leftCharacter = parseCharacter();
    if(expression->leftCharacter == '\0'){
        printError("Expected Character");
        goto fail;
    }
    for(size_t i = 0; i < 3; ++i){
        lexer_nextToken(&token);
        if(token.minor != tokenMinor_Dot){
            printError("Expected Dot");
            goto fail;
        }
    }
    expression->rightCharacter = parseCharacter();
    if(expression->rightCharacter == '\0'){
        printError("Expected Character");
        goto fail;
    }
    lexer_nextToken(&token);
    if(token.major != tokenMajor_CloseBracket){
        printError("Expected CloseBracket");
        goto fail;
    }

    if(expression->leftCharacter > expression->rightCharacter){
        printError("LeftCharacter > RightCharacter in capture group.");
        goto fail;
    }

    return expression;

fail:
    dtor_expressionCaptureGroup(expression);
    return NULL;
}

expressionTerm_t* parseTerm()
{
    expressionTerm_t* expression = NULL;
    token_t token;
    lexer_peekToken(&token);
    expression = ctor_expressionTerm();

    switch(token.major){
    case tokenMajor_OpenParen:
        expression->type = typeTerm_RegularExpression;
        lexer_nextToken(&token);
        expression->value.regularExpression = parseRegularExpressionPrime();
        if(expression->value.regularExpression == NULL){
            printError("Expected RegularExpression");
            goto fail;
        }
        lexer_nextToken(&token);
        if(token.major != tokenMajor_CloseParen){
            printError("Expected CloseParen");
            goto fail;
        }
        break;
    case tokenMajor_OpenBracket:
        expression->type = typeTerm_CaptureGroup;
        expression->value.captureGroup = parseCaptureGroup();
        if(expression->value.captureGroup == NULL){
            printError("Expected CaptureGroup");
            goto fail;
        }
        break;
    case tokenMajor_Character:
        expression->type = typeTerm_Character;
        expression->value.character = parseCharacter();
        if(expression->value.character == '\0'){
            printError("Expected Character");
            goto fail;
        }
        break;
    case tokenMajor_Epsilon:
        expression->type = typeTerm_Epsilon;
        lexer_nextToken(&token);
        break;
    default:
        printError("Unknown Expression Type");
        goto fail;
    }

    return expression;

fail:
    dtor_expressionTerm(expression);
    return NULL;
}

expressionComplement_t* parseComplement()
{
    expressionComplement_t* expression = NULL;
    token_t token;
    lexer_peekToken(&token);
    expression = ctor_expressionComplement();

    switch(token.major){
    case tokenMajor_Complement:
        lexer_nextToken(&token);
        expression->type = typeComplement_Complement;
        expression->term = parseTerm();
        if(expression->term == NULL){
            printError("Expected Term");
            goto fail;
        }
        break;
    case tokenMajor_OpenParen:
    case tokenMajor_OpenBracket:
    case tokenMajor_Character:
    case tokenMajor_Epsilon:
        expression->type = typeComplement_Term;
        expression->term = parseTerm();
        if(expression->term == NULL){
            printError("Expected Term");
            goto fail;
        }
    default:
        break;
    }

    return expression;

fail:
    dtor_expressionComplement(expression);
    return NULL;
}

expressionClosure_t* parseClosure()
{
    expressionClosure_t* expression = NULL;
    token_t token;
    expression = ctor_expressionClosure();
    lexer_peekToken(&token);

    switch(token.major){
    case tokenMajor_Complement:
    case tokenMajor_OpenParen:
    case tokenMajor_OpenBracket:
    case tokenMajor_Character:
    case tokenMajor_Epsilon:
        expression->complement = parseComplement();
        if(expression->complement == NULL){
            printError("Expected Complement");
            goto fail;
        }
        lexer_peekToken(&token);
        switch(token.major){
        case tokenMajor_Character:
            switch(token.minor){
            case tokenMinor_Star:
                expression->type = typeClosure_Kleene;
                lexer_nextToken(&token);
                break;
            case tokenMinor_Plus:
                expression->type = typeClosure_Positive;
                lexer_nextToken(&token);
                break;
            default:
                expression->type = typeClosure_Complement;
                break;
            }
            break;
        case tokenMajor_OpenCurly:
            expression->type = typeClosure_Explicit;
            lexer_nextToken(&token);
            expression->repetitionNumber = parseUInt();
            lexer_nextToken(&token);
            if(token.major != tokenMajor_CloseCurly){
                printError("CloseCurly");
                goto fail;
            }
            break;
        case tokenMajor_CloseParen:
        case tokenMajor_Or:
        case tokenMajor_Complement:
        case tokenMajor_OpenParen:
        case tokenMajor_OpenBracket:
        case tokenMajor_Epsilon:
        case tokenMajor_ExpressionSeparator:
        case tokenMajor_EOF:
            expression->type = typeClosure_Complement;
            break;
        default:
            printError("Unknown Expression Type");
            goto fail;
        }
        break;
    default:
        printError("Unknown Expression Type");
        goto fail;
    }

    return expression;

fail:
    dtor_expressionClosure(expression);
    return NULL;
}

expressionConcatenation_t* parseConcatenation()
{
    expressionConcatenation_t* expression = NULL;
    token_t token;
    lexer_peekToken(&token);
    expression = ctor_expressionConcatenation();

    switch(token.major){
    case tokenMajor_Complement:
    case tokenMajor_OpenParen:
    case tokenMajor_OpenBracket:
    case tokenMajor_Character:
    case tokenMajor_Epsilon:
        expression->type = typeConcatenation_ConcatentationClosure;
        expression->closure = parseClosure();
        if(expression->closure == NULL){
            printError("Expected Closure");
            goto fail;
        }
        expression->concatenation = parseConcatenation();
        if(expression->concatenation == NULL){
            printError("Expected Concatenation");
            goto fail;
        }
        if(expression->concatenation->type == typeConcatenation_Empty)
            expression->type = typeConcatenation_Closure;
        break;
    case tokenMajor_Or:
    case tokenMajor_CloseParen:
    case tokenMajor_ExpressionSeparator:
    case tokenMajor_EOF:
        expression->type = typeConcatenation_Empty;
        break;
    default:
        printError("Unknown Expression Type");
        goto fail;
    }

    return expression;

fail:
    dtor_expressionConcatenation(expression);
    return NULL;
}

/* NOTE: i modified this function to break the usual pattern in order to not
 * generate extra nodes in the AST. */
expressionAlternation_t* parseAlternationPrime()
{
    expressionAlternation_t* expression = NULL;
    token_t token;
    lexer_peekToken(&token);

    switch(token.major){
    case tokenMajor_Or:
        lexer_nextToken(&token);
        expression = parseAlternation();
        if(expression == NULL){
            printError("Expected Alternation");
            goto fail;
        }
        break;
    case tokenMajor_CloseParen:
    case tokenMajor_ExpressionSeparator:
    case tokenMajor_EOF:
        break;
    default:
        printError("Unknown Expression Type");
        goto fail;
    }

    return expression;

fail:
    dtor_expressionAlternation(expression);
    return NULL;
}

expressionAlternation_t* parseAlternation()
{
    expressionAlternation_t* expression = NULL;
    token_t token;
    lexer_peekToken(&token);
    expression = ctor_expressionAlternation();

    switch(token.major){
    case tokenMajor_Complement:
    case tokenMajor_OpenParen:
    case tokenMajor_OpenBracket:
    case tokenMajor_Character:
    case tokenMajor_Epsilon:
        expression->type = typeAlternation_AlternationConcatenation;
        expression->concatenation = parseConcatenation();
        if(expression->concatenation == NULL){
            printError("Expected Concatenation");
            goto fail;
        }
        expression->alternation = parseAlternationPrime();
        if(expression->alternation == NULL)
            expression->type = typeAlternation_Concatenation;
        break;
    default:
        printError("Unknown Expression Type");
        goto fail;
    }

    return expression;

fail:
    dtor_expressionAlternation(expression);
    return NULL;
}

expressionRegularExpression_t* parseRegularExpressionPrime()
{
    expressionRegularExpression_t* expression = NULL;
    token_t token;
    lexer_peekToken(&token);
    expression = ctor_expressionRegularExpression();

    switch(token.major){
    case tokenMajor_Complement:
    case tokenMajor_OpenParen:
    case tokenMajor_OpenBracket:
    case tokenMajor_Character:
    case tokenMajor_Epsilon:
        expression->type = typeRegularExpression_RegularExpression;
        expression->alternation = parseAlternation();
        if(expression->alternation == NULL){
            printError("Expected Alternation");
            goto fail;
        }
        break;
    case tokenMajor_EOF:
        expression->type = typeRegularExpression_EOF;
        break;
    default:
        printError("Unknown Expression Type");
        goto fail;
    }

    return expression;

fail:
    dtor_expressionRegularExpression(expression);
    return NULL;
}


expressionRegularExpression_t* parseRegularExpression()
{
    expressionRegularExpression_t* expression = NULL;
    token_t token;
    lexer_peekToken(&token);

    switch(token.major){
    case tokenMajor_Complement:
    case tokenMajor_OpenParen:
    case tokenMajor_OpenBracket:
    case tokenMajor_Character:
    /* Diverged from grammar */
    case tokenMajor_Epsilon:
        expression = parseRegularExpressionPrime();
        lexer_peekToken(&token);
        if(token.major == tokenMajor_ExpressionSeparator){
            lexer_nextToken(&token);
        }else if(token.major == tokenMajor_EOF){
            DO_NOTHING;
        }else{
            printError("Expected ExpressionSeparator OR EOF");
            goto fail;
        }
        break;
    case tokenMajor_ExpressionSeparator:
        expression = ctor_expressionRegularExpression();
        expression->type = typeRegularExpression_Empty;
        lexer_nextToken(&token);
        break;
    case tokenMajor_EOF:
        expression = ctor_expressionRegularExpression();
        expression->type = typeRegularExpression_EOF;
        break;
    default:
        printError("Unknown Expression Type");
        goto fail;
    }

    return expression;

fail:
    dtor_expressionRegularExpression(expression);
    return NULL;
}

void parse(char* filename)
{
    expressionRegularExpression_t* regularExpression;
    nfa_t* nfa;

    ctor_lexer(filename);

    while(true){
        regularExpression = parseRegularExpression();
        if(regularExpression == NULL){
            ccLogError("regularExpression == NULL");
            break;
        }else if(regularExpression->type == typeRegularExpression_EOF){
            break;
        }else{
            if(regularExpression->type != typeRegularExpression_Empty){
                dbg_printRE(regularExpression);
                nfa = buildNFA(regularExpression);
                dbg_printNFA(nfa);
                nfa_dtor(nfa);
            }
            dtor_expressionRegularExpression(regularExpression);
        }
    }

    dtor_expressionRegularExpression(regularExpression);
    dtor_lexer();
}