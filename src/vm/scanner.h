#ifndef JAYVM_SCAN_H
#define JAYVM_SCAN_H

#include <stdbool.h>

#include "token.h"

typedef enum {
    SCAN_SUCCESS = 0,

    // Scanner already end
    ERR_SCAN_ENDED,
    
    // No equivalent token for the character(s)
    ERR_SCAN_INV_TOKEN,
    
    // Invalid keyword
    ERR_SCAN_INV_KEYWORD,

    // Unterminated String
    ERR_SCAN_INV_STRING
} ScanError;

typedef struct {
    // the base of the source string
    char* base;
    // the start of current lexeme
    char* start;
    // current character pointer
    char* current;
    char* linestart;
    size_t line;
    size_t srcsz;
} Scanner;

ScanError scan_source(Scanner* sc, char* source, size_t source_length);
ScanError scan_token(Scanner* sc, Tkn* token);
bool scan_ended(Scanner* sc);

#endif // JAYVM_SCAN_H