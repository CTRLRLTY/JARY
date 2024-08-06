#ifndef JAYVM_AST_H
#define JAYVM_AST_H

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

typedef struct ASTExpr ASTExpr;

typedef enum {
    CON_BINARY,
    CON_UNARY,
} ConType;

typedef struct {
    TKN name;
    jary_vec_t(ASTExpr) params;
} ASTFunc;

struct ASTExpr {
    ExprType type;
    union 
    {
        TKN string;
        TKN regex;
        TKN number;
        TKN boolean;
        TKN any;
        ASTFunc func;
        jary_vec_t(TKN) property;
    } as;
};

typedef struct ASTCondition {
    ConType type;
    ASTExpr left;
    ASTExpr right;
    TKN op;
} ASTCondition;

typedef struct ASTMatch {
    ConType type;
    ASTExpr left;
    ASTExpr right;
    TKN op;
    TKN alias;
} ASTMatch;

typedef struct ASTInput {
    TKN name;
    ASTExpr expr;
} ASTInput;

typedef struct ASTTarget {
    TKN name;
    ASTExpr expr;
} ASTTarget;

typedef struct ASTRule {
    TKN name;

    jary_vec_t(ASTInput) inputs;
    jary_vec_t(ASTMatch) match;
    jary_vec_t(ASTCondition) conditions;
    jary_vec_t(ASTTarget) targets;
} ASTRule;

typedef struct ASTImport {
    TKN path;
    TKN alias;
} ASTImport;

typedef struct ASTProg {
    jary_vec_t(ASTImport) imports;
    jary_vec_t(ASTRule) rule;
} ASTProg;


void ASTProg_init(ASTProg* prog);
void ASTProg_free(ASTProg* prog);

void ASTRule_init(ASTRule* rule);
void ASTRule_free(ASTRule* rule);

void ASTFunc_init(ASTFunc* fun);
void ASTFunc_free(ASTFunc* fun);


#endif