#include "scanner.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

void jry_scan(const char  *src,
	      size_t	   length,
	      enum jy_tkn *type,
	      size_t	  *line,
	      size_t	  *ofs,
	      const char **lxstart,
	      const char **lxend)
{
#define READ	  ((size_t) (current - src))
#define ENDED()	  (READ >= length && current[0] == '\0')
#define PREV()	  (current[-1])
#define CURRENT() (current[0])
#define NEXT()	  ((*(current++)))

	const char *start   = src;
	const char *current = src;

	bool reset_ofs	    = false;

	*line		    = (*line > 0) ? *line : 1;
	*ofs		    = (*ofs > 0) ? *ofs : 1;

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
			(void) NEXT();

		if (ENDED()) {
			current = old;
			*type	= TKN_ERR_STR;
			goto END_UPDATE;
		}

		(void) NEXT(); // consume closing "
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
			(void) NEXT();
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
				(void) NEXT(); // consume '/'
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
			(void) NEXT();

		*type = TKN_NUMBER;
		goto END_UPDATE;
	}

	if (!isalpha(c) && c != '_') {
		*type = TKN_ERR;
		goto END_UPDATE;
	}

	while (!ENDED() && (isalnum(CURRENT()) || CURRENT() == '_'))
		(void) NEXT();

#define STREQ(__base, __target, __len)                                         \
	(memcmp((__base), (__target), (__len)) == 0)

	switch (start[0]) {
	case 'a':
		if (start[1] == 's')
			*type = TKN_ALIAS;
		else if (STREQ(start + 1, "ll", 2))
			*type = TKN_ALL;
		else if (STREQ(start + 1, "nd", 2))
			*type = TKN_AND;
		else if (STREQ(start + 1, "ny", 2))
			*type = TKN_ANY;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'c':
		if (STREQ(start + 1, "ondition", 8))
			*type = TKN_CONDITION;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'f':
		if (STREQ(start + 1, "alse", 4))
			*type = TKN_FALSE;
		else if (STREQ(start + 1, "ield", 4))
			*type = TKN_FIELD;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'l':
		if (STREQ(start + 1, "ong", 3))
			*type = TKN_LONG_TYPE;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'o':
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
			*type = TKN_JUMP;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'r': // rule
		if (STREQ(start + 1, "ule", 3))
			*type = TKN_RULE;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'm':
		if (STREQ(start + 1, "atch", 4))
			*type = TKN_MATCH;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 'n':
		if (STREQ(start + 1, "ot", 2))
			*type = TKN_NOT;
		else
			*type = TKN_IDENTIFIER;

		goto END_UPDATE;
	case 's':
		if (STREQ(start + 1, "tring", 5))
			*type = TKN_STRING_TYPE;
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

END_FINISH:
	*lxstart = start;
	*lxend	 = current;
}

