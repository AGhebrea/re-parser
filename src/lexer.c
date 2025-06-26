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
    lexer->buffer = NULL;
    lexer->index = 0;
    lexer->bufferCap = getpagesize();
    if((lexer->bufferCap & (lexer->bufferCap - 1)) != 0){
        lexer->bufferCap = 4096;
    }
    /* Some empirical testing on my machine showed
     * that if I don't use half pagesize buffercap 
     * i get twice the runtime for the test suite */
    lexer->bufferCap = lexer->bufferCap >> 1;
    lexer->bufferSize = 0;
    lexer->indexMask = lexer->bufferCap - 1;
    lexer->charCount = 0;
    expectExit(lexer->buffer, malloc(sizeof(char) *lexer->bufferCap), != NULL);
    expectExit(lexer->file, fopen(filename, "r"), != NULL);
    lexer->fileName = filename;
    lexer->fileEOF = false;
    lexer->lexerEOF = false;

    DebugEnabled(
        lexer->lastLoadedBuffer = 0;
        lexer->filePos = ftell(lexer->file);
    )

    (void)loadBuffer(lexer, lexer->index);
    lexer->cacheValid = false;

    return;
}

void dtor_lexer(lexer_t* lexer)
{
    free(lexer->buffer);
    if(lexer->file != NULL)
        fclose(lexer->file);
}

size_t loadBuffer(lexer_t* lexer, size_t index)
{   
    size_t readSize;

    DebugEnabled(
        if(index == 0)
            lexer->lastLoadedBuffer = 0;
        else
            lexer->lastLoadedBuffer = 1;
        ccLogTrace("Loading file at index %ld from %ld", index, lexer->filePos);
    )

    readSize = fread(&lexer->buffer[index], sizeof(char), lexer->bufferCap >> 1, lexer->file);
    if(feof(lexer->file)){
        lexer->fileEOF = true;
        if(index == 0)
            lexer->bufferSize = readSize;
        else
            lexer->bufferSize = readSize + (lexer->bufferCap >> 1);
    }else{
        lexer->bufferSize = lexer->bufferCap;
        lexer->fileEOF = false;
    }

    DebugEnabled(
        lexer->filePos = ftell(lexer->file);
    )

    return readSize;
}

/* Double Buffering with optimized modulo operation */
char nextChar(lexer_t* lexer)
{
    bool load = false;
    char character = lexer->buffer[lexer->index];

    lexer->charCount++;
    lexer->index = (lexer->index + 1) & lexer->indexMask;
    if(lexer->index == 0 || lexer->index == (lexer->bufferCap >> 1)){
        load = true;
    }
    if(lexer->index >= lexer->bufferSize || (load && lexer->fileEOF)){
        lexer->lexerEOF = true;
    }else if(lexer->lexerEOF == false && load){
        (void)loadBuffer(lexer, lexer->index);
        lexer->cacheValid = true;
    }

    return character;
}

static inline void rollBackOutsideOfCache(lexer_t* lexer, size_t rollBackSize)
{
    int status;
    size_t seek;
    size_t index;
    bool haveCache = false;
    
    lexer->lexerEOF = false;
    lexer->cacheValid = false;
    lexer->charCount -= rollBackSize;
    lexer->index = lexer->charCount & lexer->indexMask;
    seek = lexer->charCount - (lexer->index & (lexer->indexMask >> 1));
    if(lexer->charCount > lexer->bufferSize >> 1)
        haveCache = true;
    if(haveCache)
        seek -= lexer->bufferCap >> 1;
    lexer->fileEOF = false;

    DebugEnabled(
        ccLogTrace("Seeking file forward at %ld, file was at %ld", seek, lexer->filePos);
    )

    expectExit(status, fseek(lexer->file, seek, SEEK_SET), != -1);
    if(haveCache){
        /* todo: see if it saves any time over if/else
         * trick to get the other buffer offset */
        index = ((lexer->index + (lexer->bufferCap >> 1)) & lexer->indexMask) - (lexer->index & (lexer->indexMask >> 1));
        (void)loadBuffer(lexer, index);
        lexer->cacheValid = true;
    }
    /* todo: see if it saves any time over if/else
     * trick to get the current buffer offset */
    index = lexer->index - (lexer->index & (lexer->indexMask >> 1));
    (void)loadBuffer(lexer, index);
}

static inline void rollBackIntoCache(lexer_t* lexer, size_t rollBackSize)
{
    int status;
    size_t seek;

    seek = lexer->bufferCap >> 1;
    if(lexer->fileEOF)
        seek = lexer->bufferSize & (lexer->indexMask >> 1);
    lexer->cacheValid = false;
    lexer->fileEOF = false;
    lexer->lexerEOF = false;
    
    DebugEnabled(
        ccLogTrace("Seeking file backwards with %ld, file was at %ld", seek, lexer->filePos);
    )

    expectExit(status, fseek(lexer->file, -seek, SEEK_CUR), != -1);
    lexer->charCount -= rollBackSize;
    lexer->index = lexer->charCount & lexer->indexMask;
    lexer->bufferSize = lexer->bufferCap;
}

static inline void rollBackIntoMainBuffer(lexer_t* lexer, size_t rollBackSize)
{
    lexer->lexerEOF = false;
    lexer->charCount -= rollBackSize;
    lexer->index -= rollBackSize;
}

void rollBackBy(lexer_t* lexer, size_t rollBackSize)
{
    size_t maxRollback;

    if(rollBackSize > lexer->charCount)
        rollBackSize = lexer->charCount;
    if(rollBackSize == 0)
        return;
    maxRollback = lexer->index & (lexer->indexMask >> 1);
    if(lexer->cacheValid)
        maxRollback += lexer->bufferCap >> 1;
    if(rollBackSize > maxRollback){
    /* rolling outside of cache */
        rollBackOutsideOfCache(lexer, rollBackSize);
    }else{
        /* rolling either into main buffer or into cache */
        maxRollback = lexer->index & (lexer->indexMask >> 1);
        if(rollBackSize > maxRollback){
            /* rolling into cache */
            rollBackIntoCache(lexer, rollBackSize);
        }else{
            /* rolling into main buffer */
            rollBackIntoMainBuffer(lexer, rollBackSize);
        }
    }
    
    DebugEnabled(
        lexer->filePos = ftell(lexer->file);
    )
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