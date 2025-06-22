#pragma once

#include <stdio.h>
#include <stdbool.h>

typedef struct token token_t;
typedef struct lexer lexer_t;
typedef enum tokenFlairMajor tokenFlairMajor_t;
typedef enum tokenMinor tokenMinor_t;
typedef enum tokenMajor tokenMajor_t;

/* NOTE: with the major/minor scheme we can now sometimes parse the Minor tokens as chars when 
 * otherwise they should've been escaped. We can also escape them. The question is: was it 
 * worth it ? I think it is a wierd design choice because there is no way to parse a '*' that 
 * comes after any char without explicitly escaping it. For example:
 * the re a\*(^*|*+^/) and a*((^*|*+^/)) mean two differen things, where as
 * the re (^*|*+^/) and ((^*|\*+^/)) mean the same thing.
 * In any case, maybe the major/minor scheme will be useful in the future in other projects */
enum tokenMajor{
    tokenMajor_NULL = 0,
    tokenMajor_Character,
    tokenMajor_OpenParen,
    tokenMajor_CloseParen,
    tokenMajor_OpenBracket,
    tokenMajor_CloseBracket,
    tokenMajor_OpenCurly,
    tokenMajor_CloseCurly,
    tokenMajor_Epsilon,
    tokenMajor_Or,
    tokenMajor_Complement,
    tokenMajor_ExpressionSeparator,
    tokenMajor_UnknownEscape,
    tokenMajor_EOF,
};

enum tokenMinor{
    tokenMinor_NULL = 0,
    tokenMinor_Star,
    tokenMinor_Plus,
    tokenMinor_Dot,
    tokenMinor_Digit,
};

struct token{
    tokenMajor_t major;
    tokenMinor_t minor;
    char value;
};

struct lexer{
    char* buffer;
    size_t index;
    size_t fence;
    size_t bufferCap;
    size_t bufferSize;
    size_t indexMask;
    size_t charCount;
    bool fileEOF;
    bool lexerEOF;
    FILE* file;
    char* fileName;
    // debug instrumentation
    size_t lastLoadSize;
    size_t lastLoadPos;
    size_t beforeLastLoadFilePos;
    bool cacheValid;
};

void lexer_nextToken(lexer_t* lexer, token_t*);
void lexer_peekToken(lexer_t* lexer, token_t*);
void ctor_lexer(lexer_t* lexer, char* filename);
void dtor_lexer(lexer_t* lexer);
char* lexer_getTokenString(tokenMajor_t type);
size_t toktouint(token_t* token);

void rollBack(lexer_t* lexer);
char nextChar(lexer_t* lexer);
void rollBackBy(lexer_t* lexer, size_t rollBackSize);