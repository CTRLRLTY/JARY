#ifndef JAYVM_AST_H
#define JAYVM_AST_H

#include <stdint.h>
#include <stdbool.h>

#include "vector.h"
#include "token.h"


#define AS_ASTFunc(__func) _Generic((__func),                                       \
            ASTExpr*: (ASTFunc*)&((ASTExpr*)(__func))->as.func,                     \
            ASTFunc*: (ASTFunc*)(__func))

#define ASTFunc_param_size(__func) jary_vec_size(AS_ASTFunc((__func))->params)
#define ASTFunc_add_param(__func, __data) jary_vec_push(AS_ASTFunc((__func))->params, __data)


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

typedef struct ASTFunc ASTFunc;
typedef struct ASTExpr ASTExpr;

typedef enum {
    CON_BINARY,
    CON_UNARY,
} ConType;

struct ASTFunc {
    TKN name;
    jary_vec_t(ASTExpr) params;
};

struct ASTExpr {
    ExprType type;
    union val
    {
        TKN string;
        TKN regex;
        TKN number;
        TKN boolean;
        TKN any;
        ASTFunc func;
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