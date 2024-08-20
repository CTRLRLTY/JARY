#include <stdbool.h>
#include <string.h>

#include "token.h"


size_t lexsize(Tkn* token) {
  if (token == NULL)
    return 0;

  if (token->type == TKN_STRING)
    // +3 to count the quotes and '\0'
    return token->length + 3;
  
  // +1 to count '\0'
  return token->length + 1;
}

// set a cstr representation of the token lexeme
bool lexemestr(Tkn* token, char *lexeme, size_t length) {
  if (token == NULL)
    return false;

  if (lexsize(token) > length)
    return false;

  if (token->type == TKN_STRING)
    // + 2 to count the \"\"
    memcpy(lexeme, token->start, token->length + 2);
  else
    memcpy(lexeme, token->start, token->length);

  lexeme[length-1] = '\0';

  return true;
}