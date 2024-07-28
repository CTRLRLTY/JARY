#ifndef TVM_SCAN_H
#define TVM_SCAN_H

#include <stdbool.h>

#include "token.h"
#include "vector.h"

typedef enum {
    SCAN_SUCCESS = 0,

    // Scanner already end
    ERR_SCAN_ENDED,

    ERR_SCAN_DOUBLE_FREE,

    // Null pointer used as arguments
    ERR_SCAN_NULL_ARGS,
    
    // No equivalent token for the character(s)
    ERR_SCAN_INV_TOKEN,
    
    // Invalid keyword
    ERR_SCAN_INV_KEYWORD,

    // Failed to fetch custom token from vector
    ERR_SCAN_GET_CUSTOM_TKN,

    ERR_SCAN_EMPTY_THASH_VECTOR,
    ERR_SCAN_INIT_THASH_VECTOR,
    ERR_SCAN_PUSH_THASH_VECTOR,
    ERR_SCAN_FREE_THASH_VECTOR,
    // Unable to add custom token due to it already existed
    ERR_SCAN_THASH_EXISTS,

    // Unterminated String
    ERR_SCAN_INV_STRING
} ScanError;

typedef struct {
    // the start of current lexeme
    char* start;
    // current character pointer
    char* current;
    int line;
    // returning an error does not guarentee ended = true
    bool ended; 
#ifdef FEATURE_CUSTOM_TOKEN
    // enable support for custom tokens
    bool allowtknc;
    // custom token hash list
    Vector thash;
#endif // FEATURE_CUSTOM_TOKEN
} Scanner;

// must call scan_free for initialzed scanner else you dont have to
ScanError scan_init(Scanner* sc);
ScanError scan_source(Scanner* sc, char* source);
ScanError scan_token(Scanner* sc, TKN* token);
#ifdef FEATURE_CUSTOM_TOKEN
// add custom keyword. 
ScanError scan_add_name(Scanner* sc, const char* keyword, size_t keyword_length);
#endif // FEATURE_CUSTOM_TOKEN
ScanError scan_free(Scanner* sc);

#endif // TVM_SCAN_H