#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include <stdbool.h>

#include "common.h"
#include "ast.h"

typedef enum {
    PARSE_SUCCESS = 0,

    ERR_PARSE,
    ERR_PARSE_SOURCE_LENGTH,
    ERR_PARSE_NULL_ARGS,
    ERR_PARSE_UNEXPECTED_TKN,
} ParseError;

typedef union {
    ASTRule rule;
    ASTImport imprt;
} ParsedAst;

typedef enum {
    PARSED_RULE,
    PARSED_IMPORT,
    PARSED_ERR,
} ParsedType;

typedef struct {
    TKN* tkns;
    size_t tknsz;
    size_t idx;
} Parser;

ParseError parse_source(Parser* parser, TKN* tknlist, size_t length);
ParseError parse_tokens(Parser* parser, ParsedAst* ast, ParsedType* type);
bool parse_ended(Parser* parser);

#endif