#ifndef tvm_token_h
#define tvm_token_h

#include <stddef.h>
#include <stdint.h>

#define TKN Tkn
#define AS_PTKN(token) (Tkn*)&token

typedef enum {
    TKN_ERR = -1,

    TKN_EQUAL,
    TKN_LEFT_PAREN,
    TKN_RIGHT_PAREN,
    TKN_LEFT_BRACE,
    TKN_RIGHT_BRACE,
    TKN_LEFT_PBRACE,
    TKN_RIGHT_PBRACE,
    TKN_COLON,
    TKN_COMMA,
    TKN_NEWLINE,

// < LITERAL 
    TKN_STRING,
    TKN_REGEXP,
    TKN_NUMBER,
    TKN_FALSE,
    TKN_TRUE,
    TKN_ANY,
    TKN_AND,
    TKN_OR,
// > LITERAL

    TKN_IDENTIFIER,
    TKN_PVAR,
    TKN_RULE,
    TKN_INPUT,
    TKN_ALL,
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


#endif