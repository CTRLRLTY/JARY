#ifndef TVM_AST_H
#define TVM_AST_H

#include <stdint.h>
#include <stdbool.h>

#include "vector.h"
#include "token.h"

typedef enum {
    EXPR_STRING,
    EXPR_REGEX,
    EXPR_NUMBER,
    EXPR_BOOLEAN,
    EXPR_PROPERTY,
    EXPR_ANY,
    EXPR_ALL,
    EXPR_FUNC,
} ExprType;

typedef enum {
    CON_BINARY,
    CON_UNARY,
} ConType;

typedef struct {
    TKN name;
    // Vector<ASTExpr>
    Vector params;
} ASTFunc;

typedef struct {
    ExprType type;
    union 
    {
        TKN string;
        TKN regex;
        TKN number;
        TKN boolean;
        TKN any;
        ASTFunc func;
        // Vector<TKN>
        Vector property;
    } as;
} ASTExpr;

typedef struct {
    ConType type;
    ASTExpr left;
    ASTExpr right;
    TKN op;
} ASTCondition;

typedef struct {
    ConType type;
    ASTExpr left;
    ASTExpr right;
    TKN op;
    TKN alias;
} ASTMatch;

typedef struct {
    TKN name;
    ASTExpr expr;
} ASTInput;

typedef struct {
    TKN name;
    ASTExpr expr;
} ASTTarget;

typedef struct {
    TKN name;

    // Vector<ASTInput>
    Vector inputs;
    // Vector<ASTMatch>
    Vector match;
    // Vector<ASTCondition>
    Vector conditions;
    // Vector<ASTTarget>
    Vector targets;
} ASTRule;

typedef struct {
    TKN path;
    TKN alias;
} ASTImport;

typedef struct {
    // Vector<ASTImport>
    Vector imports;
    // Vector<ASTRule>
    Vector rule;
} ASTProg;


bool ASTProg_init(ASTProg* prog);
bool ASTProg_free(ASTProg* prog);
bool ASTRule_init(ASTRule* rule);
bool ASTRule_free(ASTRule* rule);
bool ASTFunc_init(ASTFunc* fun);
bool ASTFunc_free(ASTFunc* fun);


#endif