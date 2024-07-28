#ifndef TVM_PARSER_H
#define TVM_PARSER_H

#include <stdbool.h>

#include "common.h"
#include "ast.h"

typedef enum {
    PARSE_SUCCESS = 0,

    ERR_PARSE,
    ERR_PARSE_DOUBLE_FREE,
    ERR_PARSE_INIT,

    ERR_PARSE_NULL_ARGS,
    // ERR_PARSE_DOUBLE_FREE,

    // ERR_PARSE_INIT_TKNS_VECTOR,
    ERR_PARSE_EMPTY_TKNS_VECTOR,
    ERR_PARSE_GET_TKNS_VECTOR,
    ERR_PARSE_UNEXPECTED_TKN,
    ERR_PARSE_TKN_LEXEME,

    ERR_PARSE_GET_SCOPE_NAME,
    ERR_PARSE_GET_SCOPE_TYPE,
    ERR_PARSE_PUSH_SCOPE_NAME,
    ERR_PARSE_PUSH_SCOPE_TYPE,
    ERR_PARSE_POP_SCOPE_NAME,
    ERR_PARSE_POP_SCOPE_TYPE,
} ParseError;

typedef struct {
    Vector tkns;
    size_t idx;
} ParseTknStream;

typedef struct {
    // ParseScope scope;
    ParseTknStream tkns;
    bool ended;
} Parser;

ParseError parse_init(Parser* parser);
ParseError parse_tokens(Parser* parser, Vector* tkns, ASTProg* m);
ParseError parse_free(Parser* parser);

#endif