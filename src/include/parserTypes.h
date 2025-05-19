#pragma once

#include <lexer.h>

typedef enum typeRegularExpression typeRegularExpression_t;
typedef enum typeAlternation typeAlternation_t;
typedef enum typeConcatenation typeConcatenation_t;
typedef enum typeClosure typeClosure_t;
typedef enum typeComplement typeComplement_t;
typedef enum typeTerm typeTerm_t;
typedef enum typeCaptureGroup typeCaptureGroup_t;

typedef struct expressionRegularExpression expressionRegularExpression_t;
typedef struct expressionAlternation expressionAlternation_t;
typedef struct expressionConcatenation expressionConcatenation_t;
typedef struct expressionClosure expressionClosure_t;
typedef struct expressionComplement expressionComplement_t;
typedef struct expressionTerm expressionTerm_t;
typedef struct expressionCaptureGroup expressionCaptureGroup_t;

enum typeRegularExpression{
    typeRegularExpression_NULL = 0,
    typeRegularExpression_Empty,
    typeRegularExpression_EOF,
    typeRegularExpression_RegularExpression,
};
struct expressionRegularExpression{
    typeRegularExpression_t type;
    expressionAlternation_t* alternation;
};

enum typeAlternation{
    typeAlternation_NULL = 0,
    typeAlternation_Empty,
    typeAlternation_Alternation,
    typeAlternation_Concatenation,
    typeAlternation_AlternationConcatenation,
};
struct expressionAlternation{
    typeAlternation_t type;
    expressionConcatenation_t* concatenation;
    expressionAlternation_t* alternation;
};

enum typeConcatenation{
    typeConcatenation_NULL = 0,
    typeConcatenation_Empty,
    typeConcatenation_Concatentation,
    typeConcatenation_Closure,
};
struct expressionConcatenation{
    typeConcatenation_t type;
    expressionClosure_t* closure;
    expressionConcatenation_t* concatenation;
};

enum typeClosure{
    typeClosure_NULL = 0,
    typeClosure_Kleene,
    typeClosure_Positive,
    typeClosure_Explicit,
    typeClosure_Complement,
};
struct expressionClosure{
    typeClosure_t type;
    expressionComplement_t* complement;
    size_t repetitionNumber;
};

enum typeComplement{
    typeComplement_NULL = 0,
    typeComplement_Complement,
    typeComplement_Term,
};
struct expressionComplement{
    typeComplement_t type;
    expressionTerm_t* term;
};

enum typeTerm{
    typeTerm_NULL = 0,
    typeTerm_RegularExpression,
    typeTerm_CaptureGroup,
    typeTerm_Character,
    typeTerm_Epsilon,
};
union valueTerm{
    expressionRegularExpression_t* regularExpression;
    expressionCaptureGroup_t* captureGroup;
    char character;
};
struct expressionTerm{
    typeTerm_t type;
    union valueTerm value;
};

enum typeCaptureGroup{
    typeCaptureGroup_NULL = 0,
    typeCaptureGroup_CaptureGroup,
};
struct expressionCaptureGroup{
    typeCaptureGroup_t type;
    char leftCharacter;
    char rightCharacter;
};

expressionRegularExpression_t* ctor_expressionRegularExpression();
expressionAlternation_t* ctor_expressionAlternation();
expressionConcatenation_t* ctor_expressionConcatenation();
expressionClosure_t* ctor_expressionClosure();
expressionComplement_t* ctor_expressionComplement();
expressionTerm_t* ctor_expressionTerm();
expressionCaptureGroup_t* ctor_expressionCaptureGroup();

void dtor_expressionRegularExpression(expressionRegularExpression_t*);
void dtor_expressionAlternation(expressionAlternation_t*);
void dtor_expressionConcatenation(expressionConcatenation_t*);
void dtor_expressionClosure(expressionClosure_t*);
void dtor_expressionComplement(expressionComplement_t*);
void dtor_expressionTerm(expressionTerm_t*);
void dtor_expressionCaptureGroup(expressionCaptureGroup_t*);

void dbg_printRE(expressionRegularExpression_t*);