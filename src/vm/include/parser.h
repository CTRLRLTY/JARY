#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include <stdbool.h>

#include "common.h"
#include "scanner.h"
#include "ast.h"


typedef struct {
    TKN* tkns;
    Scanner* sc;
    size_t idx;
} Parser;

void jary_parse(Parser* p, ASTNode* ast, ASTMetadata* m, char* src, size_t length);

#endif