#include "scanner.h"
#include "fnv.h"
#include "error.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include <stdlib.h>

static const char* reserved_keywords[] = {
    "all",
    "action",
    "and", "or",
    "any",
    "directive", 
    "false","true",
    "forward",
    "input",
    "rule",
};

static void set_token(TKN* token, Scanner* sc, TknType type) {
    token->type = type;
    token->start = sc->start;
    token->length = (size_t) (sc->current - sc->start);
    token->offset = (size_t) (sc->current - sc->linestart);
    token->line = sc->line;
}

static TknType check_word(Scanner* sc, size_t start, size_t end, const char* word, TknType type) {
    size_t wordlen = start + end;
    char* buf = sc->start + start;

    if (sc->current - sc->start == wordlen && memcmp(buf, word, end) == 0)
        return type;

    return TKN_IDENTIFIER;
}

static ScanError set_token_name(Scanner* sc, TKN* token, TknType base) {
    while ((isalnum(*sc->current) || *sc->current == '_') && !scan_ended(sc))
        ++sc->current;

    switch (sc->start[0]) {
    case 'a':
        switch (sc->start[1]) {
            case 'l': // all
                set_token(token, sc, check_word(sc, 2, 1, "l", TKN_ALL)); break;
            case 'n': 
                switch (sc->start[2]) {
                    case 'd': // and
                        set_token(token, sc, TKN_AND); break; 
                    case 'y': // any
                        set_token(token, sc, TKN_ANY); break;    
                } break;
        } break;
    case 'c': // condition
        set_token(token, sc, check_word(sc, 1, 8, "ondition", TKN_CONDITION)); break;
    case 'f': // false
        set_token(token, sc, check_word(sc, 1, 4, "alse", TKN_FALSE)); break;
    case 'o': // or
        set_token(token, sc, check_word(sc, 1, 1, "r", TKN_OR)); break;
    case 'i': // input
        set_token(token, sc, check_word(sc, 1, 4, "nput", TKN_INPUT)); break;
    case 't': 
        switch (sc->start[1]) {
            case 'r': // true
                set_token(token, sc, check_word(sc, 2, 2, "ue", TKN_TRUE)); break;
            case 'a': // target
                set_token(token, sc, check_word(sc, 2, 4, "rget", TKN_TARGET)); break;
        } break;
    case 'r': // rule
        set_token(token, sc, check_word(sc, 1, 3, "ule", TKN_RULE)); break;
    case 'm': // match
        set_token(token, sc, check_word(sc, 1, 4, "atch", TKN_MATCH)); break;
    default: {
        // +1 to include '\0'
        size_t lexemesz = sc->current - sc->start + 1;
        char lexeme[lexemesz];

        memcpy(lexeme, sc->start, lexemesz - 1);
        lexeme[lexemesz-1] = '\0';

        uint32_t lexemehash = fnv_hash(lexeme, lexemesz); 
        token->hash = lexemehash;
        set_token(token, sc, base);
        }
    }

    return SCAN_SUCCESS;
}

ScanError scan_source(Scanner* sc, char* source, size_t length) {
    jary_assert(sc != NULL);
    jary_assert(source != NULL);
    jary_assert(length > 0);
   
    sc->base = source;
    sc->start = source;
    sc->linestart = source;
    sc->current = source;
    sc->line = 1;
    sc->srcsz = length;

    return SCAN_SUCCESS;
}

bool scan_ended(Scanner* sc) {
    return (sc->current - sc->base) >= sc->srcsz;
}

ScanError scan_token(Scanner* sc, TKN* token) {
    jary_assert(sc != NULL);
    jary_assert(token != NULL);

    if (scan_ended(sc))
        return ERR_SCAN_ENDED;

    token->type = TKN_ERR;

SCAN:
    sc->start = sc->current;

    char c = *(sc->current++);

    switch (c) {
    case '"':
        while(sc->current[0] != '"' && !scan_ended(sc))
            ++sc->current;

        if (scan_ended(sc)) 
            return ERR_SCAN_INV_STRING;

        ++sc->current; // forward enclosing "

        set_token(token, sc, TKN_STRING); 
        token->length -= 2; // -2 to not count the quotes \"\"
        return SCAN_SUCCESS;;
    case '(':
        set_token(token, sc, TKN_LEFT_PAREN); return SCAN_SUCCESS;
    case ')':
        set_token(token, sc, TKN_RIGHT_PAREN); return SCAN_SUCCESS;
    case '=': 
        set_token(token, sc, TKN_EQUAL); return SCAN_SUCCESS;
    case '{': 
        set_token(token, sc, TKN_LEFT_BRACE); return SCAN_SUCCESS;
    case '}':
        set_token(token, sc, TKN_RIGHT_BRACE); return SCAN_SUCCESS;
    case '<':
        set_token(token, sc, TKN_LESSTHAN); return SCAN_SUCCESS;
    case '>':
        set_token(token, sc, TKN_GREATERTHAN); return SCAN_SUCCESS;
    case ':':
        set_token(token, sc, TKN_COLON); return SCAN_SUCCESS;
    case ',':
        set_token(token, sc, TKN_COMMA); return SCAN_SUCCESS;

    // Ignore
    case ' ':
    case '\r':
    case '\t': 
        while (
                (   sc->current[0] == ' ' 
                    || sc->current[0] == '\r'
                    || sc->current[0] == '\t'
                ) && !scan_ended(sc)
            )
            ++sc->current;
        goto SCAN;
        
    case '\n':
        while (sc->current[0] == '\n' && !scan_ended(sc)) {
            ++sc->current;
            ++sc->line; 
        }
        
        set_token(token, sc, TKN_NEWLINE); 
        sc->linestart = sc->current;
        ++sc->line;
        return SCAN_SUCCESS;

    case '\0':
        while(sc->current[0] == '\0' && !scan_ended(sc))
            ++sc->current;
        set_token(token, sc, TKN_EOF); 
        return SCAN_SUCCESS;

    case '$':
        if (!isalpha(sc->start[1]))
            return ERR_SCAN_INV_TOKEN; 
        
        while ((isalnum(*sc->current) || *sc->current == '_') && !scan_ended(sc))
            ++sc->current;

        set_token(token, sc, TKN_PVAR); 
        return SCAN_SUCCESS;
    
    case '/': {
        char* ch = sc->current;
        while (*ch >= ' ' && *ch <= '~' && !scan_ended(sc)) {
            if (*ch == '/' && ch[-1] != '\\')
                break;
            ++ch;
        }

        if (*ch++ != '/') 
            return ERR_SCAN_INV_TOKEN;

        sc->current = ch;

        set_token(token, sc, TKN_REGEXP);
        
        return SCAN_SUCCESS;
    }

    case '1': case '2': case '3': 
    case '4': case '5': case '6':
    case '7': case '8': case '9':
        while (isdigit(*sc->current) && !scan_ended(sc))
            ++sc->current;

        set_token(token, sc, TKN_NUMBER); 
        return SCAN_SUCCESS;
    default:
        if (!isalpha(c) && c != '_')
            return ERR_SCAN_INV_TOKEN;

        return set_token_name(sc, token, TKN_IDENTIFIER);
    }

    return ERR_SCAN_INV_TOKEN;
}