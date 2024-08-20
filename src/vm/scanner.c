#include "scanner.h"
#include "fnv.h"
#include "error.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include <stdlib.h>

static void set_token(Tkn* token, Scanner* sc, TknType type) {
    token->type = type;
    token->start = sc->start;
    token->linestart = sc->linestart;
    token->length = (size_t) (sc->current - sc->start);
    token->offset = (size_t) (sc->current - sc->linestart);
    token->offset -= token->length;
    token->offset += 1;
    token->line = sc->line;
}

static TknType check_word(Scanner* sc, size_t start, size_t end, const char* word, TknType type) {
    size_t wordlen = start + end;
    char* buf = sc->start + start;

    if (sc->current - sc->start == wordlen && memcmp(buf, word, end) == 0)
        return type;

    return TKN_IDENTIFIER;
}

static void set_token_name(Scanner* sc, Tkn* token) {
    while ((isalnum(*sc->current) || *sc->current == '_') && !scan_ended(sc))
        ++sc->current;

    switch (sc->start[0]) {
    case 'a':
        switch (sc->start[1]) {
            case 'l': // all
                set_token(token, sc, check_word(sc, 2, 1, "l", TKN_ALL)); return;
            case 'n': 
                switch (sc->start[2]) {
                    case 'd': // and
                        set_token(token, sc, TKN_AND); return; 
                    case 'y': // any
                        set_token(token, sc, TKN_ANY); return;    
                } break;
        } break;
    case 'c': // condition
        set_token(token, sc, check_word(sc, 1, 8, "ondition", TKN_CONDITION)); return;
    case 'f': // false
        set_token(token, sc, check_word(sc, 1, 4, "alse", TKN_FALSE)); return;
    case 'o': // or
        set_token(token, sc, check_word(sc, 1, 1, "r", TKN_OR)); return;
    case 'i':
        switch (sc->start[1]) {
        case 'n':
            switch (sc->start[2]) {
            case 'c': // include
                set_token(token, sc, check_word(sc, 3, 4, "lude", TKN_INCLUDE)); return;
            case 'g': // ingress
                set_token(token, sc, check_word(sc, 3, 4, "ress", TKN_INGRESS)); return;
            case 'p': // input
                set_token(token, sc, check_word(sc, 3, 2, "ut", TKN_INPUT)); return;
            } 
            
            break;
        // case 'm':
        //     set_token(token, sc, check_word(sc, 2, 4, "port", TKN_IMPORT)); return;
        } break;
    case 't': 
        switch (sc->start[1]) {
            case 'r': // true
                set_token(token, sc, check_word(sc, 2, 2, "ue", TKN_TRUE)); return;
            case 'a': // target
                set_token(token, sc, check_word(sc, 2, 4, "rget", TKN_TARGET)); return;
        } break;
    case 'r': // rule
        set_token(token, sc, check_word(sc, 1, 3, "ule", TKN_RULE)); return;
    case 'm': // match
        set_token(token, sc, check_word(sc, 1, 4, "atch", TKN_MATCH)); return;
    }

    // +1 to include '\0'
    size_t lexemesz = sc->current - sc->start + 1;
    char lexeme[lexemesz];

    memcpy(lexeme, sc->start, lexemesz - 1);
    lexeme[lexemesz-1] = '\0';

    uint32_t lexemehash = fnv_hash(lexeme, lexemesz); 
    token->hash = lexemehash;
    set_token(token, sc, TKN_IDENTIFIER);
}

void scan_source(Scanner* sc, char* source, size_t length) {
    jary_assert(sc != NULL);
    jary_assert(source != NULL);
    jary_assert(length > 0);
   
    sc->base = source;
    sc->start = source;
    sc->linestart = source;
    sc->current = source;
    sc->line = 1;
    sc->srcsz = length;
}

bool scan_ended(Scanner* sc) {
    return (sc->current - sc->base) >= sc->srcsz;
}

void scan_token(Scanner* sc, Tkn* token) {
    jary_assert(sc != NULL);
    jary_assert(token != NULL);

    if (scan_ended(sc))
        goto INV_TKN;

SCAN:
    sc->start = sc->current;

    char c = *(sc->current++);

    switch (c) {
    case '"': {
        char* ch = sc->current;

        while(!scan_ended(sc) && *sc->current != '"') {
            ++sc->current;
        }

        if (scan_ended(sc)) {
            sc->current = ch;
            set_token(token, sc, TKN_ERR_STR);
            return;
        }

        ++sc->current; // forward enclosing "
        set_token(token, sc, TKN_STRING); 
        token->length -= 2; // -2 to not count the quotes \"\"
        return;
    }
    case '(':
        set_token(token, sc, TKN_LEFT_PAREN); return;
    case ')':
        set_token(token, sc, TKN_RIGHT_PAREN); return;
    case '=': 
        set_token(token, sc, TKN_EQUAL); return;
    case '!':
        set_token(token, sc, TKN_BANG); return;
    case '{': 
        set_token(token, sc, TKN_LEFT_BRACE); return;
    case '}':
        set_token(token, sc, TKN_RIGHT_BRACE); return;
    case '<':
        set_token(token, sc, TKN_LESSTHAN); return;
    case '>':
        set_token(token, sc, TKN_GREATERTHAN); return;
    case ':':
        set_token(token, sc, TKN_COLON); return;
    case '+':      
        set_token(token, sc, TKN_PLUS); return;
    case '-':      
        set_token(token, sc, TKN_MINUS); return;
    case '*':      
        set_token(token, sc, TKN_STAR); return;
    case '.':
        set_token(token, sc, TKN_DOT); return;
    case ',':
        set_token(token, sc, TKN_COMMA); return;  
    case '$':
        set_token(token, sc, TKN_DOLLAR); return;

    // Ignore
    case ' ':
    case '\r':
    case '\t': 
        while (
                !scan_ended(sc)                     &
                (   
                    sc->current[0] == ' '           ||
                    sc->current[0] == '\r'          ||
                    sc->current[0] == '\t'
                )
            )
            ++sc->current;
        goto SCAN;
        
    case '\n':
        while (!scan_ended(sc) && sc->current[0] == '\n') {
            ++sc->current;
            ++sc->line; 
        }
        
        set_token(token, sc, TKN_NEWLINE); 
        sc->linestart = sc->current;
        ++sc->line;
        return;

    case '\0':
        while(!scan_ended(sc) && sc->current[0] == '\0')
            ++sc->current;

        set_token(token, sc, TKN_EOF); 
        return;
    
    case '/': {
        char* ch = sc->current;
        while (*sc->current >= ' ' && *sc->current <= '~' && !scan_ended(sc)) {
            if (*sc->current == '/' && sc->current[-1] != '\\')
                break;
            ++sc->current;
        }

        if (*(sc->current++) != '/') {
            sc->current = ch;

            set_token(token, sc, TKN_SLASH);
            return;
        }

        set_token(token, sc, TKN_REGEXP);
        
        return;
    }

    case '1': case '2': case '3': 
    case '4': case '5': case '6':
    case '7': case '8': case '9':
        while (!scan_ended(sc) && isdigit(*sc->current))
            ++sc->current;

        set_token(token, sc, TKN_NUMBER); 
        return;
    default:
        if (!isalpha(c) && c != '_') {
            goto INV_TKN;
        }

        set_token_name(sc, token);
        return;
    }

INV_TKN:
    set_token(token, sc, TKN_ERR);
}