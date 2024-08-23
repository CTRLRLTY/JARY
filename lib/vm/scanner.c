#include "scanner.h"

#include "memory.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

size_t jry_scan(const char *src, size_t length, jy_tkn_type_t *type,
		size_t *line, size_t *ofs, char **lexeme, size_t *lexsz)
{
#define READ	  ((size_t) (current - src))
#define ENDED()	  (READ >= length && current[0] == '\0')
#define PREV()	  (current[-1])
#define CURRENT() (current[0])
#define NEXT()	  ((*(current++)))

	const char *current   = src;
	const char *start     = src;
	bool	    reset_ofs = false;

	*line		      = (*line > 0) ? *line : 1;
	*ofs		      = (*ofs > 0) ? *ofs : 1;

	if (ENDED()) {
		*type = TKN_EOF;
		goto END_FINISH;
	}

SCAN:
	start  = current;
	char c = NEXT();

	switch (c) {
	case '"': {
		const char *old = current;

		while (!ENDED() && CURRENT() != '"')
			NEXT();

		if (ENDED()) {
			current = old;
			*type	= TKN_ERR_STR;
			goto END_UPDATE;
		}

		NEXT(); // consume closing "
		*type = TKN_STRING;
		goto END_UPDATE;
	}
	case '(':
		*type = TKN_LEFT_PAREN;
		goto END_UPDATE;
	case ')':
		*type = TKN_RIGHT_PAREN;
		goto END_UPDATE;
	case '=':
		*type = TKN_EQUAL;
		goto END_UPDATE;
	case '{':
		*type = TKN_LEFT_BRACE;
		goto END_UPDATE;
	case '}':
		*type = TKN_RIGHT_BRACE;
		goto END_UPDATE;
	case '<':
		*type = TKN_LESSTHAN;
		goto END_UPDATE;
	case '>':
		*type = TKN_GREATERTHAN;
		goto END_UPDATE;
	case ':':
		*type = TKN_COLON;
		goto END_UPDATE;
	case '~':
		*type = TKN_TILDE;
		goto END_UPDATE;
	case '+':
		*type = TKN_PLUS;
		goto END_UPDATE;
	case '-':
		*type = TKN_MINUS;
		goto END_UPDATE;
	case '*':
		*type = TKN_STAR;
		goto END_UPDATE;
	case '.':
		*type = TKN_DOT;
		goto END_UPDATE;
	case ',':
		*type = TKN_COMMA;
		goto END_UPDATE;
	case '$':
		*type = TKN_DOLLAR;
		goto END_UPDATE;

	// Ignore
	case ' ':
	case '\r':
	case '\t':
		goto SCAN;

	case '\n':
		*line += 1;

		while (!ENDED() && CURRENT() == '\n') {
			NEXT();
			*line += 1;
		}

		*type	  = TKN_NEWLINE;
		reset_ofs = true;
		goto END_UPDATE;

	case '\0':
		*type = TKN_EOF;
		goto END_FINISH;

	case '/': {
		const char *old = current;

		do {
			if (CURRENT() == '/' && PREV() != '\\') {
				*type = TKN_REGEXP;
				NEXT(); // consume '/'
				goto END_UPDATE;
			}
		} while (!ENDED() && CURRENT() >= ' ' && NEXT() <= '~');

		current = old;
		*type	= TKN_SLASH;

		goto END_UPDATE;
	}

	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		while (!ENDED() && isdigit(CURRENT()))
			NEXT();

		*type = TKN_NUMBER;
		goto END_UPDATE;
	}

	if (!isalpha(c) && c != '_') {
		*type = TKN_ERR;
		goto END_UPDATE;
	}

	while (!ENDED() && isalnum(CURRENT()) || CURRENT() == '_')
		NEXT();

#define STREQ(__base, __target, __len)                                         \
	(memcmp((__base), (__target), (__len)) == 0)

	switch (start[0]) {
	case 'a':
		if (STREQ(start + 1, "ll", 2))
			*type = TKN_ALL;
		else if (STREQ(start + 1, "nd", 2))
			*type = TKN_AND;
		else if (STREQ(start + 1, "ny", 2))
			*type = TKN_ANY;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'c': // condition
		if (STREQ(start + 1, "ondition", 8))
			*type = TKN_CONDITION;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'f': // false
		if (STREQ(start + 1, "alse", 4))
			*type = TKN_FALSE;
		else if (STREQ(start + 1, "ields", 5))
			*type = TKN_FIELDS;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'o': // or
		if (start[1] == 'r')
			*type = TKN_OR;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'i':
		if (STREQ(start + 1, "nclude", 6))
			*type = TKN_INCLUDE;
		else if (STREQ(start + 1, "ngress", 6))
			*type = TKN_INGRESS;
		else if (STREQ(start + 1, "nput", 4))
			*type = TKN_INPUT;
		else if (STREQ(start + 1, "mport", 5))
			*type = TKN_IMPORT;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 't':
		if (STREQ(start + 1, "rue", 3))
			*type = TKN_TRUE;
		else if (STREQ(start + 1, "arget", 5))
			*type = TKN_TARGET;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'r': // rule
		if (STREQ(start + 1, "ule", 3))
			*type = TKN_RULE;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'm': // match
		if (STREQ(start + 1, "atch", 4))
			*type = TKN_MATCH;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'n': // not
		if (STREQ(start + 1, "ot", 2))
			*type = TKN_NOT;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	}

	*type = TKN_IDENTIFIER;

END_UPDATE:
	if (reset_ofs)
		*ofs = 1;
	else
		*ofs += READ;

	*lexsz	= (size_t) (current - start) + 1;
	*lexeme = jry_alloc(*lexsz);
	memcpy(*lexeme, start, *lexsz);
	(*lexeme)[*lexsz - 1] = '\0';

END_FINISH:
	return READ;
}