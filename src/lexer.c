#include "cclog_macros.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <cclog.h>
#include <ccstd.h>
#include <lexer.h>
#include <ccdebug.h>

size_t loadBuffer(lexer_t* lexer, size_t index);

void ctor_lexer(lexer_t* lexer, char* filename)
{
    expect(lexer->file, fopen(filename, "rb"), != NULL);
    lexer->charCount = 0;
    lexer->fileName = filename;
    if(feof(lexer->file))
        lexer->lexerEOF = true;
    else
        lexer->lexerEOF = false;

    return;
}

void dtor_lexer(lexer_t* lexer)
{
    if(lexer->file != NULL)
        fclose(lexer->file);
}

/* Double Buffering with optimized modulo operation */
char nextChar(lexer_t* lexer)
{
    char character;

    DebugEnabled(
        assert(lexer->lexerEOF != true);
    )

    lexer->charCount += 1;
    character = getc(lexer->file);
    if(feof(lexer->file))
        lexer->lexerEOF = true;

    return character;
}

void rollBackBy(lexer_t* lexer, size_t rollBackSize)
{
    int status;
    size_t pos = ftell(lexer->file);

    (void)status;
    if(rollBackSize > pos)
        rollBackSize = pos;
    lexer->charCount -= rollBackSize;
    expect(status, fseek(lexer->file, -rollBackSize, SEEK_CUR), != -1);
}

bool discardChar(char ch)
{
    if(ch == ' ' || ch == '\n')
        return false;
    return isspace(ch);
}

void lexer_nextToken(lexer_t* lexer, token_t* token)
{
    char* RESpecialChars = "()[]{}?*+|.^";
    char* whitespaceSpecialChars = "fnrtv";
    char ch = '\0';
    token->major = tokenMajor_NULL;
    token->minor = tokenMinor_NULL;

    if(lexer->lexerEOF == true){
        token->major = tokenMajor_EOF;
        return;
    }
    
    ch = nextChar(lexer);
    while(discardChar(ch))
        ch = nextChar(lexer);

    if(ch == '\n'){
        token->major = tokenMajor_ExpressionSeparator;
    }else if(strchr(RESpecialChars, ch) != NULL){
        switch (ch) {
        case '(':
            token->major = tokenMajor_OpenParen;
            break;
        case ')':
            token->major = tokenMajor_CloseParen;
            break;
        case '[':
            token->major = tokenMajor_OpenBracket;
            break;
        case ']':
            token->major = tokenMajor_CloseBracket;
            break;
        case '{':
            token->major = tokenMajor_OpenCurly;
            break;
        case '}':
            token->major = tokenMajor_CloseCurly;
            break;
        case '?':
            token->major = tokenMajor_Epsilon;
            break;
        case '*':
            token->major = tokenMajor_Character;
            token->minor = tokenMinor_Star;
            break;
        case '+':
            token->major = tokenMajor_Character;
            token->minor = tokenMinor_Plus;
            break;
        case '|':
            token->major = tokenMajor_Or;
            break;
        case '.':
            token->major = tokenMajor_Character;
            token->minor = tokenMinor_Dot;
            break;
        case '^':
            token->major = tokenMajor_Complement;
            break;
        default:
            break;
        }
        token->value = ch;
    }else if(ch == '\\'){
        ch = nextChar(lexer);
        if(strchr(RESpecialChars, ch) != NULL){
            token->major = tokenMajor_Character;
            token->value = ch;
        }else if(strchr(whitespaceSpecialChars, ch) != NULL){
            switch(ch){
            case 'f':
                ch = '\f';
                break;
            case 'n':
                ch = '\n';
                break;
            case 'r':
                ch = '\r';
                break;
            case 't':
                ch = '\t';
                break;
            case 'v':
                ch = '\v';
                break;
            default:
                ccLogError("???");
                exit(1);
            }
            token->major = tokenMajor_Character;
            token->value = ch;
        }else{
            token->major = tokenMajor_UnknownEscape;
        }
    }else if(!isspace(ch)){
        if(isdigit(ch))
            token->minor = tokenMinor_Digit;
        token->major = tokenMajor_Character;
        token->value = ch;
    }
}

void lexer_peekToken(lexer_t* lexer, token_t* token)
{
    size_t initialCharCount = lexer->charCount;
    lexer_nextToken(lexer, token);
    size_t finalCharCount = lexer->charCount;
    rollBackBy(lexer, finalCharCount - initialCharCount);
}

char* lexer_getTokenString(tokenMajor_t type)
{
    switch(type){
    case tokenMajor_NULL:
        return "<NULL>";
        break;
    case tokenMajor_Character:
        return "Character";
        break;
    case tokenMajor_OpenParen:
        return "OpenParen";
        break;
    case tokenMajor_CloseParen:
        return "CloseParen";
        break;
    case tokenMajor_OpenBracket:
        return "OpenBracket";
        break;
    case tokenMajor_CloseBracket:
        return "CloseBracket";
        break;
    case tokenMajor_OpenCurly:
        return "OpenCurly";
        break;
    case tokenMajor_CloseCurly:
        return "CloseCurly";
        break;
    case tokenMajor_Epsilon:
        return "Epsilon";
        break;
    case tokenMajor_UnknownEscape:
        return "UnknonwEscapeSequence";
        break;
    case tokenMajor_Or:
        return "Or";
        break;
    case tokenMajor_Complement:
        return "Complement";
        break;
    case tokenMajor_EOF:
        return "<EOF>";
        break;
    default:
        return "<UNKNONW TOKEN>";
    }

    return "<UNKNONW TOKEN>";
}