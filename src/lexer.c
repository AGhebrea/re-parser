#include "cclog_macros.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <cclog.h>
#include <ccstd.h>
#include <lexer.h>

size_t __index = 0;

size_t loadBuffer(lexer_t* lexer, size_t index, size_t size)
{   
    size_t readSize = 0;

    readSize = fread(&lexer->buffer[index], sizeof(char), size, lexer->file);
    if(feof(lexer->file)){
        lexer->fileEOF = true;
        if(index >= (lexer->bufferCap >> 1)){
            lexer->bufferSize = (lexer->bufferCap >> 1) + readSize;
        }else{
            lexer->bufferSize = readSize;
        }
    }else{
        lexer->bufferSize = lexer->bufferCap;
        lexer->fileEOF = false;
    }

    return readSize;
}

void ctor_lexer(lexer_t* lexer, char* filename)
{
    lexer->buffer = NULL;
    lexer->index = 0;
    lexer->fence = 0;
    lexer->bufferCap = getpagesize();
    /* is this even needed ? */
    if((lexer->bufferCap & (lexer->bufferCap - 1)) != 0){
        lexer->bufferCap = 4096;
    }
    lexer->bufferSize = 0;
    lexer->indexMask = lexer->bufferCap - 1;
    lexer->charCount = 0;
    expectExit(lexer->buffer, malloc(sizeof(char) *lexer->bufferCap), != NULL);
    expectExit(lexer->file, fopen(filename, "r"), != NULL);
    lexer->fileName = filename;
    lexer->fileEOF = false;
    lexer->lexerEOF = false;
    (void)loadBuffer(lexer, lexer->index, lexer->bufferCap >> 1);

    return;
}

void dtor_lexer(lexer_t* lexer)
{
    free(lexer->buffer);
    if(lexer->file != NULL)
        fclose(lexer->file);
}

void rollBack(lexer_t* lexer)
{
    int stat;
    size_t loadSize = 0;
    long seekSize = 0;
    bool seek = false;

    if(lexer->charCount == 0)
        return;

    /* we seek back on these two values because we want to load buffer again 
    * on the next nextChar call */
    if(lexer->index == (lexer->bufferCap >> 1) - 1){
        seekSize = (long)(lexer->bufferCap >> 1) - 1;
        seek = true;
    }else if(lexer->index == 0){
        // (!!!) skip the null terminator character
        lexer->index = lexer->bufferCap - 1;
        seekSize = (long)(lexer->bufferCap >> 1);
        seek = true;
    }else if(lexer->fence == lexer->index){
        if(lexer->fence == 0){
            lexer->fence = (lexer->bufferCap >> 1);
        }else{
            lexer->fence = 0;
        }
        loadSize = lexer->bufferCap - 1;
        seekSize = loadSize;
        ccLogDebug("Seek'd backwards with: %ld; charcount = %ld", seekSize, lexer->charCount);
        ccLogDebug("file offset before: %ld", lexer->file->_offset);
        expectExit(stat, fseek(lexer->file, -seekSize, SEEK_CUR), != -1 );
        ccLogDebug("file offset after: %ld", lexer->file->_offset);
        ccLogDebug("Loading buffer at %ld with loadsize = %ld from %ld", lexer->fence, loadSize, ftell(lexer->file));
        loadBuffer(lexer, lexer->fence, loadSize);
    }
    if(seek){
        if(lexer->fileEOF == true){
            seekSize = lexer->bufferSize & (lexer->indexMask >> 1);
        }
        ccLogDebug("Seek'd backwards with: %ld; charcount = %ld", seekSize, lexer->charCount);
        ccLogDebug("file offset before: %ld", lexer->file->_offset);
        expectExit(stat, fseek(lexer->file, -seekSize, SEEK_CUR), != -1 );
        ccLogDebug("file offset after: %ld", lexer->file->_offset);
        lexer->fileEOF = false;
    }

    lexer->index = (lexer->index - 1) & lexer->indexMask;
    lexer->charCount--;
    lexer->lexerEOF = false;
    if(lexer->charCount == 0)
        lexer->index = 0;

    return;
}

void rollBackBy(lexer_t* lexer, size_t rollBackSize)
{
    size_t cursor;
    size_t seek;
    size_t charCount;
    size_t loadSize;
    size_t loadIndex; 
    size_t mainBufferLowBound;
    size_t mainBufferHighBound;
    size_t maxRollback;
    bool mainBufferIsFirst = false;
    bool loadCache = false;
    int stat;

    ccLogDebug("%ld", __index);
    __index++;

    if(rollBackSize == 0)
        return;

    if(rollBackSize >= lexer->charCount)
    {
        lexer->index = 0;
        lexer->fence = 0;
        lexer->charCount = 0;
        lexer->lexerEOF = false;
        return;
    }

    // maybe we can order these in reverse and have them all be if
    // so that what needs to happen happens

    cursor = (lexer->index - rollBackSize) & lexer->indexMask;
    // we need to skip the null terminator char
    if(cursor == (lexer->bufferCap - 1))
        cursor -= 1;
    // because again, if we have less data than 0x1000 this is a special case
    if(lexer->fence == 0 && lexer->charCount >= lexer->bufferCap >> 1){
        maxRollback = lexer->index + (lexer->bufferCap >> 1) - 1;
    }else{
        maxRollback = lexer->index;
    }

    // we re rolling back into cached buffer, we want to set fence and seek back the 
    // filedescr state
    if(rollBackSize < maxRollback){
        // todo: isn't there an easier check now ? there should be
        /* this is a special case as the main buffer could be either one 
         * depending on how much data is inside of the buffer */
        if(lexer->fence == 0 && lexer->charCount > lexer->bufferCap >> 1){
            mainBufferLowBound = (lexer->bufferCap >> 1) - 1;
            mainBufferHighBound = lexer->bufferCap - 1;
        }else{
            mainBufferLowBound = 0;
            // -2 because of the way we load the buffer in nextChar
            mainBufferHighBound = (lexer->bufferCap >> 1) - 2;
        }
        // if we are rolling back into same buffer then do nothing
        if(cursor >= mainBufferLowBound && cursor < mainBufferHighBound){
            /* do nothing. if else case is better with this */
        }else{
            ccLogDebug("lexer eof: %d", lexer->lexerEOF);
            ccLogDebug("file eof: %d", lexer->fileEOF);
            if(lexer->fileEOF == true){
                if(lexer->fence == 0)
                    seek = (lexer->bufferSize - (lexer->bufferCap >> 1));
                else
                    seek = lexer->bufferSize;
                lexer->fileEOF = false;
            }else{
                if(lexer->fence == 0){
                    seek = (lexer->bufferCap >> 1) - 1;
                }else{
                    seek = lexer->bufferCap >> 1;
                    // need to skip null terminatior char
                    // cursor -= 1;
                }
            }
            ccLogDebug("Seek'd backwards with: %ld; charcount = %ld", seek, lexer->charCount);
            ccLogDebug("file offset before: %ld", lexer->file->_offset);
            expectExit(stat, fseek(lexer->file, -seek, SEEK_CUR), != -1);
            ccLogDebug("file offset after: %ld", lexer->file->_offset);
        }
    // we re rolling back outside cached buffer, since we need to read from file again
    // it is easier to just load the lexer state in place;
    }else{
        cursor -= 1;
        charCount = lexer->charCount - rollBackSize;
        if(cursor < lexer->bufferCap >> 1)
            mainBufferIsFirst = true;
        if(charCount > lexer->bufferCap >> 1)
            loadCache = true;

        seek = charCount - cursor;
        if(mainBufferIsFirst && loadCache){
            seek -= (lexer->bufferCap >> 1) - 1;
        }
        ccLogDebug("Seek'd DIRECTLY at: %ld; charcount = %ld", seek, lexer->charCount);
        ccLogDebug("file offset before: %ld", lexer->file->_offset);
        expectExit(stat, fseek(lexer->file, seek, SEEK_SET), != -1);
        ccLogDebug("file offset after: %ld", lexer->file->_offset);
        // loading cache and setting fence
        if(loadCache){
            if(mainBufferIsFirst){
                loadSize = (lexer->bufferCap >> 1) - 1;
                loadIndex = lexer->bufferCap >> 1;
                lexer->fence = loadIndex;
            }else{
                loadSize = lexer->bufferCap >> 1;
                loadIndex = 0;
                lexer->fence = loadIndex;
            }
            ccLogDebug("Loading CACHE buffer at %ld with loadsize = %ld; from %ld ", loadIndex, loadSize, ftell(lexer->file));
            loadBuffer(lexer, loadIndex, loadSize);
        }else{
            if(mainBufferIsFirst){
                lexer->fence = 0;
            }else{
                lexer->fence = lexer->bufferCap >> 1;
            }
        }
        // loading main buffer
        if(mainBufferIsFirst){
            loadIndex = 0;
            loadSize = lexer->bufferCap >> 1;
        }else{
            loadIndex = lexer->bufferCap >> 1;
            loadSize = (lexer->bufferCap >> 1) - 1;
        }
        ccLogDebug("Loading MAIN buffer at %ld with loadsize = %ld; from %ld", loadIndex, loadSize, ftell(lexer->file));
        loadBuffer(lexer, loadIndex, loadSize);
    }

    lexer->index = cursor;
    lexer->charCount -= rollBackSize;
    lexer->lexerEOF = false;
}

/* Double Buffering with optimized modulo operation */
char nextChar(lexer_t* lexer)
{
    size_t nextIndex;
    size_t loadSize;
    bool load = false;
    char character = lexer->buffer[lexer->index];

    lexer->charCount++;
    lexer->index = (lexer->index + 1) & lexer->indexMask;
    nextIndex = (lexer->index + 1) & lexer->indexMask;
    if(nextIndex == 0){
        lexer->fence = (lexer->bufferCap >> 1);
        loadSize = (lexer->bufferCap >> 1);
        // (!!!) skip the null terminator character
        lexer->index = nextIndex;
        load = true;
    }else if(nextIndex == (lexer->bufferCap >> 1)){
        lexer->fence = 0;
        loadSize = (lexer->bufferCap >> 1) - 1;
        load = true;
    }
    if(nextIndex > lexer->bufferSize || (load && lexer->fileEOF)){
        lexer->lexerEOF = true;
    }else if(lexer->lexerEOF == false && load){
        ccLogDebug("[%p] Loading buffer at %ld with loadsize = %ld from %ld", lexer, nextIndex, loadSize, ftell(lexer->file));
        (void)loadBuffer(lexer, nextIndex, loadSize);
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
    for(size_t i = initialCharCount; i < finalCharCount; ++i)
        rollBack(lexer);
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