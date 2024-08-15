#ifndef JAYVM_AST_H
#define JAYVM_AST_H

#include <stdint.h>
#include <stdbool.h>

#include "token.h"


typedef enum {
    AST_ROOT,

    AST_RULE,
    AST_INPUT,
    AST_CALL,

    AST_ASSIGNMENT,
    
    AST_FIELD,
    AST_EVENT,

    AST_NAME,
    AST_LITERAL,
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
    size_t number;
    TKN* tkn;
    struct ASTNode* child;
} ASTNode;

typedef struct ASTMetadata {
    // tkns array
    TKN* tkns;
    size_t tknsz;

    // error node array
    ASTError* errors;
    size_t errsz;

    // total graph size
    size_t size;
} ASTMetadata;

void ast_free(ASTNode* ast);
size_t ast_degree(ASTNode* ast);
void ast_meta_free(ASTMetadata* m);

#endif