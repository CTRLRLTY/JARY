#ifndef JAYVM_SCAN_H
#define JAYVM_SCAN_H

#include <stdbool.h>

#include "token.h"

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

void scan_source(Scanner* sc, char* source, size_t source_length);
void scan_token(Scanner* sc, Tkn* token);
bool scan_ended(Scanner* sc);

#endif // JAYVM_SCAN_H