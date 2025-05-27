#include "cclog_macros.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <cclog.h>
#include <lexer.h>

lexer_t lexer;

size_t loadBuffer(size_t index, size_t size)
{   
    size_t readSize = 0;

    readSize = fread(&lexer.buffer[index], sizeof(char), size, lexer.file);
    if(feof(lexer.file)){
        lexer.fileEOF = true;
        if(index >= (lexer.bufferCap >> 1)){
            lexer.bufferSize = (lexer.bufferCap >> 1) + readSize;
        }else{
            lexer.bufferSize = readSize;
        }
    }else{
        lexer.bufferSize = lexer.bufferCap;
        lexer.fileEOF = false;
    }

    return readSize;
}

void ctor_lexer(char* filename)
{
    lexer.buffer = NULL;
    lexer.index = 0;
    lexer.fence = 0;
    lexer.bufferCap = getpagesize();
    /* is this even needed ? */
    if((lexer.bufferCap & (lexer.bufferCap - 1)) != 0){
        lexer.bufferCap = 4096;
    }
    // Debug value:
    // lexer.bufferCap = 16;
    lexer.indexMask = lexer.bufferCap - 1;
    lexer.charCount = 0;
    expectExit(lexer.buffer, malloc(sizeof(char) *lexer.bufferCap), != NULL);
    expectExit(lexer.file, fopen(filename, "r"), != NULL);
    (void)loadBuffer(lexer.index, lexer.bufferCap >> 1);
    lexer.fileName = filename;

    return;
}

void dtor_lexer()
{
    free(lexer.buffer);
    if(lexer.file != NULL)
        fclose(lexer.file);
}

void rollBack()
{
    int stat;
    size_t loadSize = 0;
    long seekSize = 0;
    bool seek = false;

    if(lexer.charCount == 0)
        return;

    if(lexer.index == (lexer.bufferCap >> 1) - 1){
        seekSize = (long)(lexer.bufferCap >> 1) - 1;
        seek = true;
    }else if(lexer.index == 0){
        // (!!!) skip the null terminator character
        lexer.index = lexer.bufferCap - 1;
        seekSize = (long)(lexer.bufferCap >> 1);
        seek = true;
    }else if(lexer.fence == lexer.index){
        if(lexer.fence == 0){
            lexer.fence = (lexer.bufferCap >> 1);
        }else{
            lexer.fence = 0;
        }
        loadSize = lexer.bufferCap - 1;
        seekSize = loadSize;
        expectExit(stat, fseek(lexer.file, -seekSize, SEEK_CUR), != -1 );
        loadBuffer(lexer.fence, loadSize);
    }
    if(seek){
        if(lexer.fileEOF == true){
            seekSize = lexer.bufferSize & (lexer.indexMask >> 1);
        }
        expectExit(stat, fseek(lexer.file, -seekSize, SEEK_CUR), != -1 );
        lexer.fileEOF = false;
    }

    lexer.index = (lexer.index - 1) & lexer.indexMask;
    lexer.charCount--;
    lexer.lexerEOF = false;
    if(lexer.charCount == 0)
        lexer.index = 0;

    return;
}

/* Double Buffering with optimized modulo operation */
char nextChar()
{
    size_t nextIndex;
    size_t loadSize;
    bool load = false;
    char character = lexer.buffer[lexer.index];

    lexer.charCount++;
    lexer.index = (lexer.index + 1) & lexer.indexMask;
    nextIndex = (lexer.index + 1) & lexer.indexMask;
    if(nextIndex == 0){
        lexer.fence = (lexer.bufferCap >> 1);
        loadSize = (lexer.bufferCap >> 1);
        // (!!!) skip the null terminator character
        lexer.index = nextIndex;
        load = true;
    }else if(nextIndex == (lexer.bufferCap >> 1)){
        lexer.fence = 0;
        loadSize = (lexer.bufferCap >> 1) - 1;
        load = true;
    }
    if(nextIndex > lexer.bufferSize || (load && lexer.fileEOF)){
        lexer.lexerEOF = true;
    }else if(lexer.lexerEOF == false && load){
        (void)loadBuffer(nextIndex, loadSize);
    }

    return character;
}

bool discardChar(char ch)
{
    if(ch == ' ' || ch == '\n')
        return false;
    return isspace(ch);
}

/* TODO: think of an exhaustive test list for the nextChar + rollback mechanism
 * but for now LGTM
 * TODO: document */
void lexer_nextToken(token_t* token)
{
    char* RESpecialChars = "()[]{}?*+|.^";
    char* whitespaceSpecialChars = "fnrtv";
    char ch = '\0';
    token->major = tokenMajor_NULL;
    token->minor = tokenMinor_NULL;

    if(lexer.lexerEOF == true){
        token->major = tokenMajor_EOF;
        return;
    }
    
    ch = nextChar();
    while(discardChar(ch))
        ch = nextChar();

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
        ch = nextChar();
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

void lexer_peekToken(token_t* token)
{
    size_t initialCharCount = lexer.charCount;
    lexer_nextToken(token);
    size_t finalCharCount = lexer.charCount;
    for(size_t i = initialCharCount; i < finalCharCount; ++i)
        rollBack();
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