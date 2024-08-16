#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include <stdbool.h>

#include "scanner.h"
#include "ast.h"


typedef struct {
    TKN** tkns;
    Scanner* sc;

    // Current Basic Block.
    //
    // This field is changed when entering
    // a new production rule and it is callee-save.
    size_t block;
    
    size_t idx;

    size_t depth;
} Parser;

void jary_parse(Parser* p, ASTNode* ast, ASTMetadata* m, const char* src, size_t length);

#endif