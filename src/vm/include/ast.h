#ifndef JAYVM_AST_H
#define JAYVM_AST_H

#include <stdint.h>
#include <stdbool.h>

#include "vector.h"
#include "token.h"


typedef enum {
    AST_ERR,
    AST_ROOT,
    AST_RULE,
    AST_NAME,
    AST_LITERAL,
    AST_FUNC,
    AST_CALL,
    AST_ASSIGNMENT,
    AST_EVENT,
    AST_FIELD,
    AST_INPUT,
    AST_VAR,
} ASTType;

typedef struct ASTError {
    TknType got;
    TknType expect;
    size_t line;
    size_t offset;
    size_t length;
    char* lexeme;
    char* msg;
} ASTError;

typedef struct ASTNode {
    ASTType type;
    union value {
        TKN* tkn;
        ASTError* err;
        jary_vec_t(struct ASTNode) params;
        jary_vec_t(struct ASTNode) sections;
        jary_vec_t(struct ASTNode) vars;
        jary_vec_t(struct ASTNode) decls;
    } value;
    struct ASTNode* left;
    struct ASTNode* right;
} ASTNode;

void ast_free(ASTNode* ast);


#endif