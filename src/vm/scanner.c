#include "scanner.h"
#include "fnv.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

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
    token->line = sc->line;
}

static TknType check_word(Scanner* sc, size_t start, size_t end, const char* word, TknType type) {
    size_t wordlen = start + end;
    char* buf = sc->start + start;

    if (sc->current - sc->start == wordlen && memcmp(buf, word, end) == 0)
        return type;

    return TKN_ERR;
}

static ScanError set_token_name(Scanner* sc, TKN* token, TknType base) {
    token->type = TKN_ERR;

    while (isalnum(*sc->current) || *sc->current == '_')
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
        case 't': // true
            set_token(token, sc, check_word(sc, 1, 3, "rue", TKN_TRUE)); break;
        case 'r': // rule
            set_token(token, sc, check_word(sc, 1, 3, "ule", TKN_RULE)); break;
        case 'm':
            set_token(token, sc, check_word(sc, 1, 4, "atch", TKN_MATCH)); break;
    }

    if (token->type == TKN_ERR) {
        // +1 to include '\0'
        size_t lexemesz = sc->current - sc->start + 1;
        char lexeme[lexemesz];

        memcpy(lexeme, sc->start, lexemesz - 1);
        lexeme[lexemesz-1] = '\0';

        uint32_t lexemehash = fnv_hash(lexeme, lexemesz); 
        token->hash = lexemehash;
        set_token(token, sc, base);

#ifdef FEATURE_CUSTOM_TOKEN
        if (sc->allowtknc) {
            if (sc->thash.count <= 0) 
                return ERR_SCAN_EMPTY_THASH_VECTOR;

            if (vec_find(&sc->thash, &lexemehash, sizeof(lexemehash), NULL, 0) == VEC_FOUND) {
                set_token(token, sc, TKN_CUSTOM);
            }
        }
#endif // FEATURE_CUSTOM_TOKEN
    }

    return SCAN_SUCCESS;
}

ScanError scan_init(Scanner* sc) {
    if (sc == NULL)
        return ERR_SCAN_NULL_ARGS;

    sc->start = NULL;
    sc->current = NULL;
    sc->line = 0;
    sc->ended = true;

#ifdef FEATURE_CUSTOM_TOKEN
    sc->allowtknc = false;
    sc->thash.capacity += VEC_INITIAL_CAPACITY;

    for (size_t i = 0; i < sizeof(reserved_keywords)/sizeof(reserved_keywords[0]); ++i) {
        if (vec_init(&sc->thash, sizeof(uint32_t)) != VEC_SUCCESS)
            return ERR_SCAN_INIT_THASH_VECTOR;

        uint32_t thash = fnv_hash(reserved_keywords[i], strlen(reserved_keywords[i]));
        if (vec_push(&sc->thash, &thash, sizeof(thash)) != VEC_SUCCESS)
            return ERR_SCAN_PUSH_THASH_VECTOR;
    }
#endif

    return SCAN_SUCCESS;
}

ScanError scan_source(Scanner* sc, char* source) {
    if (sc == NULL || source == NULL)
        return ERR_SCAN_NULL_ARGS;
   
    sc->start = source;
    sc->current = source;
    sc->line = 1;
    sc->ended = false;

    return SCAN_SUCCESS;
}

ScanError scan_free(Scanner* sc) {
    if (sc == NULL)
        return ERR_SCAN_NULL_ARGS;

    if (sc->start == NULL
#ifdef FEATURE_CUSTOM_TOKEN
        && sc->thash.data == NULL
#endif // FEATURE_CUSTOM_TOKEN
        ) return ERR_SCAN_DOUBLE_FREE;

#ifdef FEATURE_CUSTOM_TOKEN
    if (!sc->thash.count)
        return ERR_SCAN_EMPTY_THASH_VECTOR;
    
    if (vec_free(&sc->thash) != VEC_SUCCESS)
        return ERR_SCAN_FREE_THASH_VECTOR;
#endif // FEATURE_CUSTOM_TOKEN

    sc->start = NULL;
    sc->current = NULL;
    sc->ended = true;
    sc->line = 0;

    return SCAN_SUCCESS;
}

ScanError scan_token(Scanner* sc, TKN* token) {
    if (sc->ended)
        return ERR_SCAN_ENDED;

    if (sc == NULL || token == NULL)
        return ERR_SCAN_NULL_ARGS;

    token->type = TKN_ERR;

SCAN:
    sc->start = sc->current;

    char c = *(sc->current++);

    switch (c) {
        case '"':
            while(sc->current[0] != '"' && sc->current[0] != '\0')
                ++sc->current;

            if (sc->current[0] == '\0') {
                sc->ended = true;
                return ERR_SCAN_INV_STRING;
            }

            ++sc->current; // forward enclosing "

            set_token(token, sc, TKN_STRING); 
            token->length -= 2; // -2 to not count the quotes \"\"
            break;
        case '(':
            set_token(token, sc, TKN_LEFT_PAREN); break;
        case ')':
            set_token(token, sc, TKN_RIGHT_PAREN); break;
        case '=': 
            set_token(token, sc, TKN_EQUAL); break;
        case '{': 
            set_token(token, sc, TKN_LEFT_BRACE); break;
        case '}':
            set_token(token, sc, TKN_RIGHT_BRACE); break;
        case '<':
            set_token(token, sc, TKN_LEFT_PBRACE); break;
        case '>':
            set_token(token, sc, TKN_RIGHT_PBRACE); break;
        case ':':
            set_token(token, sc, TKN_COLON); break;
        case ',':
            set_token(token, sc, TKN_COMMA); break;

        // Ignore
        case ' ':
        case '\r':
        case '\t': 
            while (sc->current[0] == ' ' || sc->current[0] == '\r' || sc->current[0] == '\t')
                ++sc->current;
            goto SCAN;
            // set_token(token, sc, TKN_WHITESPACE); break;
        case '\n':
            while (sc->current[0] == '\n')
                ++sc->current;

            set_token(token, sc, TKN_NEWLINE); 
            ++sc->line; break;
        case '\0':
            sc->ended = true;
            set_token(token, sc, TKN_EOF); break;
        case '$':
            if (!isalpha(sc->start[1]))
                return ERR_SCAN_INV_TOKEN; 
            
            while (isalnum(*sc->current) || *sc->current == '_')
                ++sc->current;

            set_token(token, sc, TKN_PVAR); break;
        case '1': case '2': case '3': 
        case '4': case '5': case '6':
        case '7': case '8': case '9':
            while (isdigit(*sc->current))
                ++sc->current;

            set_token(token, sc, TKN_NUMBER); break;
        default:
            if (!isalpha(c) && c != '_')
                return ERR_SCAN_INV_TOKEN;

            return set_token_name(sc, token, TKN_IDENTIFIER);
    }
    
    return SCAN_SUCCESS;
}

#ifdef FEATURE_CUSTOM_TOKEN
ScanError scan_add_name(Scanner* sc, const char* keyword, size_t keylen) {
    sc->allowtknc = true;

    uint32_t thash = fnv_hash(keyword, keylen);

    if (vec_find(&sc->thash, &thash, sizeof(thash), NULL, 0) == VEC_FOUND)
        return ERR_SCAN_THASH_EXISTS;

    vec_push(&sc->thash, &thash, sizeof(thash));

    return SCAN_SUCCESS;
}
#endif // FEATURE_CUSTOM_TOKEN