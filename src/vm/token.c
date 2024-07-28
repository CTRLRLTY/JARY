#include <stdbool.h>
#include <string.h>

#include "token.h"


size_t tkn_lexeme_size(TKN* token) {
  if (token->type == TKN_STRING)
    // +3 to count the quotes and '\0'
    return token->length + 3;
  
  // +1 to count '\0'
  return token->length + 1;
}

// set a cstr representation of the token lexeme
bool tkn_lexeme(TKN* token, char *lexeme, size_t lexeme_size) {
  if (tkn_lexeme_size(token) > lexeme_size)
    return false;

  if (token->type == TKN_STRING)
    // + 2 to count the \"\"
    memcpy(lexeme, token->start, token->length + 2);
  else
    memcpy(lexeme, token->start, token->length);

  lexeme[lexeme_size-1] = '\0';

  return true;
}