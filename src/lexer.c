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

size_t __index = 0;

size_t loadBuffer(lexer_t* lexer, size_t index, size_t size)
{   
    size_t readSize = 0;
    int status = 0;

    expectExit(status, ftell(lexer->file), != -1);

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

    lexer->lastLoadSize = 0;
    lexer->lastLoadPos = 0;
    lexer->beforeLastLoadFilePos = 0;
    lexer->cacheValid = false;

    return;
}

void dtor_lexer(lexer_t* lexer)
{
    free(lexer->buffer);
    if(lexer->file != NULL)
        fclose(lexer->file);
}

// todo: make buffers symmetric because it is silly otherwise.
/* Double Buffering with optimized modulo operation */
char nextChar(lexer_t* lexer)
{
    size_t status = 0;
    size_t nextIndex;
    size_t loadSize;
    bool load = false;
    char character = lexer->buffer[lexer->index];

    if(lexer->charCount < lexer->bufferCap - 1)
        assert(lexer->charCount == lexer->index);
    else
        assert(lexer->charCount > lexer->index);
    assert(lexer->lexerEOF == false);
    expectExit(status, ftell(lexer->file), != -1);

    ccLogDebug("index: %ld; charcount: %ld", lexer->index, lexer->charCount);

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
        ccLogDebug("index: %ld; charcount: %ld", lexer->index, lexer->charCount);
        ccLogDebug("[%p] Loading buffer at %ld with loadsize = %ld from %ld", lexer, nextIndex, loadSize, ftell(lexer->file));
        lexer->beforeLastLoadFilePos = ftell(lexer->file);
        lexer->lastLoadSize = loadBuffer(lexer, nextIndex, loadSize);
        lexer->lastLoadPos = nextIndex;
        lexer->cacheValid = true;
    }

    return character;
}

/* - first buffer is delimited from [0, 0x7fe)
 * - second buffer is [0x7fe, 0xffe) */
static inline bool isFirstBuffer(size_t index, size_t bufferCap)
{
    if((index >= 0 && index < (bufferCap >> 1) - 1))
        return true;
    return false;
}

/* some of this could've been easier if we just made
 * the two buffers symmetric. We could've just read 
 * 1 less actual byte from file so that both loads 
 * are the same size. We were the architect of 
 * our downfall all along. Another solution would 
 * be to store more information inside of lexer and 
 * use that for rollbacks, for example the information
 * that we use for debug instrumentation. */ 
/* 3 cases, we always roll back from main buffer
 * case 1: we roll back into main buffer;
 *      - we don't have to seek back the buffer
 * case 2: we roll back into cache;
 *      - we have to seek back the buffer
 * case 3: we roll outside of cache; 
 *      - we have to construct the lexer state from 0
 * - if lexer->index == 0x7fe when nextChar is called, 
 * - it will load the second buffer, in this case lexer->index
 * becomes 0x7ff and is still part of first buffer. In contrast
 * to the other case. A discrepancy but I left that in as a small
 * optimization, we need to deal with that
 * - if lexer->index == 0xffe when nextChar is called,
 * (!!!) from the two facts above, in case two, when
 * when in range [0x7ff, 0xffe] and the next cursor 
 * becomes <= 0x7fe, we need to seek back the file.
 * if we are in range [0, 0x7fe] and next cursor 
 * becomes <= 0xffe, we need to seek back the file.
 * - it will load the first buffer, lexer->index becomes 0, 
 * part of first buffer
 * - first buffer is delimited from [0, 0x7fe)
 * - second buffer is [0x7fe, 0xffe)
 * - 0xfff is always NULL character if the second buffer 
 * is full with data
 * - we load the first buffer with 0x800 bytes
 * - we load the second buffer with 0x7ff bytes
 * - if we are in first buffer, we might not have cache */
void rollBackBy(lexer_t* lexer, size_t rollBackSize)
{
    ccLogDebug("[%ld] rollBackSize: %ld", __index, rollBackSize);

    int status = 0;
    size_t seek;
    size_t cursor;
    size_t maxRollBack;
    size_t charCount;
    size_t index;
    size_t cacheIndex;
    size_t loadSize;
    size_t cacheLoadSize;
    size_t fixup;
    bool rollingInsideOfFirstBuffer = false;
    bool rollingInsideOfMainBuffer = false;
    bool rollingOutsideOfCache = false;
    bool mainBufferIsFirst = false;
    bool haveCache = false;
    
    expectExit(status, ftell(lexer->file), != -1);

    /* nothing to do */
    if(lexer->charCount == 0 || rollBackSize == 0)
        return;

    haveCache = lexer->cacheValid;

    if(isFirstBuffer(lexer->index, lexer->bufferCap)){
        mainBufferIsFirst = true;
        if(haveCache)
            maxRollBack = lexer->index + (lexer->bufferCap >> 1) - 1;
        else{
            maxRollBack = lexer->index;
        }
    }else{
        if(haveCache){
            maxRollBack = lexer->index;
        }else{
            maxRollBack = lexer->index - (lexer->bufferCap >> 1);
        }
    };
    
    if(rollBackSize > maxRollBack)
        rollingOutsideOfCache = true;

    cursor = (lexer->index - rollBackSize) & lexer->indexMask;

    if(rollingOutsideOfCache){
        ccLogDebug("[case 1] RollingOutsideOfCache");
        /* since we already need to load data from file, 
         * we just reconstruct the lexer state from 0 */     
        if(rollBackSize > lexer->charCount)
            rollBackSize = lexer->charCount;
        charCount = lexer->charCount - rollBackSize;
        cursor = charCount & lexer->indexMask;
        if(charCount >= (lexer->bufferCap >> 1))
            haveCache = true;
        else
            haveCache = false;

        /* we need to fixup the lexer->charCount because: 
         * for ex. with buffercap == 0x1000 then
         * charcount of 0x17ff would come at index 0x800
         * charcount of 0x27ff would come at index 0x801
         * ... so on
         * to fixup:
         * fixup = (charCount / lexer->bufferCap)
         * and theoretically can be so big that it 
         * overflows multiple times, must be accounted for.
         * */
        fixup = (charCount / (lexer->bufferCap - 1)) & lexer->indexMask;
        /* we need to set lexer->index here because if that happens we have to 
         * make the main buffer be the first one and we need to set have cache */
        lexer->index = (cursor + fixup) & lexer->indexMask;
        if(lexer->index >= lexer->bufferCap - 1){
            if(lexer->index == lexer->bufferCap - 1)
                lexer->index = 0;
            else
                lexer->index -= lexer->bufferCap;
            haveCache = true;
            mainBufferIsFirst = true;
        }
        if(isFirstBuffer(lexer->index, lexer->bufferCap))
            mainBufferIsFirst = true;
        else
            mainBufferIsFirst = false;
        /* we also seek back the file
         * 4 cases, if we haveCache combined with mainBufferIsFirst */
        if(haveCache){
            if(mainBufferIsFirst){
                seek = charCount - lexer->index - ((lexer->bufferCap >> 1) - 1);
                loadSize = lexer->bufferCap >> 1;
                cacheLoadSize = (lexer->bufferCap >> 1) - 1;
                index = 0;
                cacheIndex = lexer->bufferCap >> 1;
            }else{
                seek = charCount - lexer->index;
                loadSize = (lexer->bufferCap >> 1) - 1;
                cacheLoadSize = lexer->bufferCap >> 1;
                index = lexer->bufferCap >> 1;
                cacheIndex = 0;
            }
        }else{
            if(mainBufferIsFirst){
                seek = 0;
                loadSize = lexer->bufferCap >> 1;
                cacheLoadSize = 0;
                index = 0;
                cacheIndex = 0;
            }else{
                seek = 0;
                loadSize = (lexer->bufferCap >> 1) - 1;
                cacheLoadSize = 0;
                index = lexer->bufferCap >> 1;
                cacheIndex = 0;
            }
        }
        lexer->fileEOF = false;
        ccLogDebug("[%ld] rollBackSize: %ld", __index, rollBackSize);
        ccLogDebug("[%p] seeking file forward, with seek size: %ld; pos = %ld", lexer, seek, ftell(lexer->file));
        expectExit(status, fseek(lexer->file, seek, SEEK_SET), != -1);
        ccLogDebug("[%p] after seek pos = %ld", lexer, ftell(lexer->file));
        if(haveCache){
            ccLogDebug("[%p] Loading CACHE buffer at %ld with loadsize = %ld from %ld", lexer, cacheIndex, cacheLoadSize, ftell(lexer->file));
            (void)loadBuffer(lexer, cacheIndex, cacheLoadSize);
            lexer->cacheValid = true;
        }else {
            lexer->cacheValid = false;
        }
        /* another special case */
        if(seek == 0 && haveCache == false){
            index = 0;
            loadSize = lexer->bufferCap >> 1;
        }
        ccLogDebug("[%p] Loading MAIN buffer at %ld with loadsize = %ld from %ld", lexer, index, loadSize, ftell(lexer->file));
        (void)loadBuffer(lexer, index, loadSize);
        lexer->beforeLastLoadFilePos = ftell(lexer->file) - loadSize;
        lexer->lastLoadSize = loadSize;
        lexer->lastLoadPos = index;
        /* we also manually set lexer state here */
        lexer->charCount = charCount;
        lexer->fence = cacheIndex;
    }else{
        /* we need to figure out if we are rolling back into cache or main buffer */
        if(isFirstBuffer(cursor, lexer->bufferCap))
            rollingInsideOfFirstBuffer = true;
        if((rollingInsideOfFirstBuffer && mainBufferIsFirst) || ((rollingInsideOfFirstBuffer == false && mainBufferIsFirst == false)))
            rollingInsideOfMainBuffer = true;
        
        /* a hack for the moment to handle the stupid case where we are in index 0x7fe, we technically are not in first buffer but we 
         * also have cache because of the way this code works */
        if(rollingInsideOfMainBuffer == false && haveCache == false)
            rollingInsideOfMainBuffer = true;
        
        if(rollingInsideOfMainBuffer){
            /* do nothing */
            ccLogDebug("[case 2] rollingInsideOfMainBuffer");
        }else if(haveCache){
            ccLogDebug("[case 3] rollingOutsideOfMainBuffer and haveCache");
            charCount = lexer->charCount - rollBackSize;
            /* if mainBufferIsFirst and we are rolling outside of mainBuffer
             * then the last loaded buffer was secondBuffer, we need to seek 
             * back by 0x7ff, otherwise we seek back by 0x800 
             * then mainBuffer becomes cache buffer */
            if(mainBufferIsFirst){
                /* we seek back by 0x7ff or remainder if that is the case */
                if(lexer->fileEOF)
                    seek = lexer->bufferSize;
                else
                    seek = (lexer->bufferCap >> 1);
                cacheIndex = 0;
                cacheLoadSize = lexer->bufferCap >> 1;
                /* we need to skip NULL terminator char */
                cursor -= 1;
            }else{
                /* we seek back by 0x800 or remainder if that is the case */
                if(lexer->fileEOF)
                    seek = lexer->bufferSize - (lexer->bufferCap >> 1);
                else
                    seek = (lexer->bufferCap >> 1) - 1;
                cacheIndex = lexer->bufferCap >> 1;
                cacheLoadSize = (lexer->bufferCap >> 1) - 1;
            }
            /* we need to seek back the file */
            lexer->fileEOF = false;
            ccLogDebug("[%ld] rollBackSize: %ld", __index, rollBackSize);
            ccLogDebug("[%p] seeking file backwards, with seek size: %ld; pos = %ld", lexer, seek, ftell(lexer->file));
            // assert(lexer->lastLoadSize == seek);
            seek = lexer->lastLoadSize;
            expectExit(status, fseek(lexer->file, -seek, SEEK_CUR), != -1);
            assert(ftell(lexer->file) == lexer->beforeLastLoadFilePos);
            lexer->cacheValid = false;
            ccLogDebug("[%p] after seek pos = %ld", lexer, ftell(lexer->file));
        }else{
            ccLogDebug("[case 4] rollingOutsideOfMainBuffer and DOES NOT haveCache");
            /* do nothing */
        }
        if(haveCache == false && rollingInsideOfMainBuffer == false){
            /* zero out lexer state */
            lexer->index = 0;
            lexer->charCount = 0;
            lexer->fence = 0;
        }else{
            /* update lexer state */
            if(cursor == lexer->bufferCap)
                cursor -= 1;
            lexer->index = cursor;
            lexer->charCount -= rollBackSize;
            if(rollingInsideOfMainBuffer == false){
                lexer->fence = 0;
            }
        }
    }

    lexer->lexerEOF = false;

    __index++;
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