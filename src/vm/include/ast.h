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

    AST_NAME,
    AST_LITERAL,
} ASTType;

typedef struct ASTError {
    size_t line;
    size_t offset;
    size_t length;
    char* lexeme;
    char* linestr;
    char* msg;
} ASTError;

typedef struct ASTNode {
    ASTType type;
    size_t id;
    TKN* tkn;
    struct ASTNode* child;
} ASTNode;

// BBs are section and arguments
typedef struct BasicBlock {
    TKN* def;
    TKN* use;
} BasicBlock;

typedef struct ASTMetadata {
    char* src;

    // tkns array
    TKN** tkns;
    size_t tknsz;

    // Basic block table
    size_t* bbkey;
    BasicBlock* bbval;
    size_t bbsz;

    // error node array
    ASTError* errors;
    size_t errsz;

    // total graph size
    size_t size;
    // maximum depth
    size_t depth;
} ASTMetadata;

void ast_free(ASTNode* ast);
size_t ast_degree(ASTNode* ast);
void ast_meta_free(ASTMetadata* m);

BasicBlock* find_basic_block(size_t** bbkey, BasicBlock** bbval, size_t block);

#endif