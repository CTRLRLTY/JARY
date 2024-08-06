#ifndef JAYVM_TOKEN_H
#define JAYVM_TOKEN_H

#include <stddef.h>
#include <stdint.h>

#define TKN Tkn
#define AS_PTKN(token) (Tkn*)&token

typedef enum {
    TKN_ERR = -1,

    TKN_LEFT_PAREN,
    TKN_RIGHT_PAREN,
    TKN_LEFT_BRACE,
    TKN_RIGHT_BRACE,
    TKN_COLON,
    TKN_COMMA,
    TKN_NEWLINE,

// < OPERATOR SYMBOL
    TKN_EQUAL,
    TKN_LESSTHAN,
    TKN_GREATERTHAN,
// > OPERATOR SYMBOL

// < LITERAL 
    TKN_STRING,
    TKN_REGEXP,
    TKN_NUMBER,
    TKN_FALSE,
    TKN_TRUE,
    TKN_ANY,
    TKN_AND,
    TKN_OR,
    TKN_ALL,
// > LITERAL

    TKN_IDENTIFIER,
    TKN_PVAR,
    TKN_RULE,
    TKN_INPUT,
    TKN_TARGET,
    TKN_CONDITION,
    TKN_MATCH,

    TKN_CUSTOM,

    TKN_EOF,
} TknType;

typedef struct {
    TknType type;
    size_t length; // Does not count the quotes ("")
    size_t line;
    char* start;
    uint32_t hash;
} Tkn;

size_t tkn_lexeme_size(TKN* token);

// set a cstr representation of the token lexeme
bool tkn_lexeme(TKN* token, char *lexeme, size_t lexeme_size);


#endif // JAYVM_TOKEN_H