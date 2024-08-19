#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include <stdbool.h>

#include "ast.h"

void jary_parse(ASTNode* ast, ASTMetadata* m, const char* src, size_t length);

#endif