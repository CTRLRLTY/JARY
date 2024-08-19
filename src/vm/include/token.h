#ifndef JAYVM_TOKEN_H
#define JAYVM_TOKEN_H

#include <stddef.h>
#include <stdint.h>

#define AS_PTKN(token) (Tkn*)&token

typedef enum TknType {
    TKN_ERR,
    TKN_ERR_STR,

    TKN_LEFT_PAREN,
    TKN_RIGHT_PAREN,
    TKN_LEFT_BRACE,
    TKN_RIGHT_BRACE,
    TKN_DOT,
    TKN_COMMA,
    TKN_COLON,
    TKN_NEWLINE,

    TKN_TARGET,
    TKN_INPUT,
    TKN_MATCH,
    TKN_CONDITION,

    TKN_RULE,
    TKN_IMPORT,
    TKN_INGRESS,

// < OPERATOR SYMBOL
    TKN_EQUAL,
    TKN_LESSTHAN,
    TKN_GREATERTHAN,
    TKN_ANY,
    TKN_AND,
    TKN_OR,
    TKN_ALL,
// > OPERATOR SYMBOL

// < LITERAL 
    TKN_REGEXP,
    TKN_STRING,
    TKN_NUMBER,
    TKN_FALSE,
    TKN_TRUE,
// > LITERAL

    TKN_IDENTIFIER,
    TKN_DOLLAR,

    TKN_CUSTOM,
    TKN_EOF,
} TknType;

typedef struct Tkn {
    TknType type;
    size_t length; // Does not count the quotes ("")
    size_t line;
    size_t offset;
    char* start;
    char* linestart;
    uint32_t hash;
} Tkn;

size_t tkn_lexeme_size(Tkn* token);

// set a cstr representation of the token lexeme
bool tkn_lexeme(Tkn* token, char *lexeme, size_t lexeme_size);


#endif // JAYVM_TOKEN_H