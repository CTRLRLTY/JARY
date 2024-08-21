#ifndef JAYVM_AST_H
#define JAYVM_AST_H

#include <stdint.h>
#include <stdbool.h>

#include "token.h"


typedef enum {
    AST_ROOT,

    AST_DECL,
    AST_SECTION,

    AST_BINARY,
    AST_CALL,
    AST_UNARY,
    AST_EVENT,
    AST_MEMBER,

    AST_NAME,
    AST_PATH,
    AST_LITERAL,
} ASTType;

typedef struct ASTError {
    size_t line;
    size_t offset;
    size_t length;
    char* lexeme;
    char* msg;
} ASTError;

typedef struct ASTNode {
    ASTType type;
    // id is assigned on the order of it being processed
    size_t id;
    Tkn* tkn;
    struct ASTNode* child;
    size_t degree;
} ASTNode;

typedef struct ASTMetadata {
    char* src;

    // tkns array
    Tkn** tkns;
    size_t tknsz;

    // error node array
    ASTError* errors;
    size_t errsz;

    // total graph size
    size_t size;
    // maximum depth
    size_t depth;
} ASTMetadata;

void free_ast(ASTNode* ast);
void free_ast_meta(ASTMetadata* m);

#endif