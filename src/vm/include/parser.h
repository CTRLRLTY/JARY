#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include <stdbool.h>

#include "common.h"
#include "ast.h"


typedef struct {
    TKN* tkns;
    size_t tknsz;
    size_t idx;
} Parser;

void parse_tokens(Parser* parser, ASTNode* ast, TKN* tknlist, size_t length);

#endif