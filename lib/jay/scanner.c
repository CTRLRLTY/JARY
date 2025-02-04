/*
BSD 3-Clause License

Copyright (c) 2024. Muhammad Raznan. All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "scanner.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

const char *jry_scan(const char *start, uint32_t length, enum jy_tkn *type)
{
#define READ	  ((uint32_t) (current - start))
#define ENDED()	  (READ >= length || current[0] == '\0')
#define PREV()	  (current[-1])
#define CURRENT() (current[0])
#define NEXT()	  ((*(current++)))

	const char *current = start;

	if (READ >= length) {
		*type = TKN_EOF;
		goto FINISH;
	}

	char c = NEXT();

	switch (c) {
	case '"': {
		const char *old = current;

		while (!ENDED() && CURRENT() != '"' && CURRENT() != '\n')
			(void) NEXT();

		if (ENDED() || CURRENT() == '\n') {
			current = old;
			*type	= TKN_ERR_STR;
			goto FINISH;
		}

		(void) NEXT(); // consume closing "
		*type = TKN_STRING;
		goto FINISH;
	}
	case '(':
		*type = TKN_LEFT_PAREN;
		goto FINISH;
	case ')':
		*type = TKN_RIGHT_PAREN;
		goto FINISH;
	case '=':
		if (*current != '=')
			break;
		current += 1;
		*type	 = TKN_EQ;
		goto FINISH;
	case '{':
		*type = TKN_LEFT_BRACE;
		goto FINISH;
	case '}':
		*type = TKN_RIGHT_BRACE;
		goto FINISH;
	case '<':
		*type = TKN_LESSTHAN;
		goto FINISH;
	case '>':
		*type = TKN_GREATERTHAN;
		goto FINISH;
	case ':':
		*type = TKN_COLON;
		goto FINISH;
	case '~':
		*type = TKN_TILDE;
		goto FINISH;
	case '+':
		*type = TKN_PLUS;
		goto FINISH;
	case '-':
		*type = TKN_MINUS;
		goto FINISH;
	case '*':
		*type = TKN_STAR;
		goto FINISH;
	case '|':
		*type = TKN_VERTBAR;
		goto FINISH;
	case '\\':
		switch (*current) {
		case 'n':
			*type	 = TKN_LF;
			current += 1;
			goto FINISH;
		case 't':
			*type	 = TKN_HT;
			current += 1;
			goto FINISH;
		case 'r':
			*type	 = TKN_CR;
			current += 1;
			goto FINISH;
		case 'f':
			*type	 = TKN_FF;
			current += 1;
			goto FINISH;
		default:
			*type = TKN_BACKSLASH;
			goto FINISH;
		}

	case '^':
		*type = TKN_CARET;
		goto FINISH;
	case '?':
		*type = TKN_QMARK;
		goto FINISH;
	case '.':
		if (*current == '.') {
			*type	 = TKN_CONCAT;
			current += 1;
		} else {
			*type = TKN_DOT;
		}

		goto FINISH;
	case '[':
		*type = TKN_LEFT_BRACKET;
		goto FINISH;
	case ']':
		*type = TKN_RIGHT_BRACKET;
		goto FINISH;
	case ',':
		*type = TKN_COMMA;
		goto FINISH;
	case '$':
		*type = TKN_DOLLAR;
		goto FINISH;

	// Ignore
	case ' ':
	case '\r':
	case '\t':
		*type = TKN_SPACES;

		while (!ENDED()) {
			switch (CURRENT()) {
			case ' ':
			case '\r':
			case '\t':
				(void) NEXT();
				continue;
			default:
				goto FINISH;
			}
		}

		goto FINISH;
	case '/': {
		const char *old = current;

		if (CURRENT() == '/') {
			while (CURRENT() != '\n')
				(void) NEXT();
			*type = TKN_COMMENT;
			goto FINISH;
		}

		do {
			if (CURRENT() == '/' && PREV() != '\\') {
				*type = TKN_REGEXP;
				(void) NEXT(); // consume '/'
				goto FINISH;
			}
		} while (!ENDED() && CURRENT() >= ' ' && NEXT() <= '~');

		current = old;
		*type	= TKN_SLASH;

		goto FINISH;
	}
	case '\n':
		while (!ENDED() && CURRENT() == '\n')
			(void) NEXT();

		*type = TKN_NEWLINE;
		goto FINISH;
	case '\0':
		*type = TKN_EOF;
		goto FINISH;

	case '0':
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

		switch (current[0]) {
		case 'h':
			current++;
			*type = TKN_HOUR;
			break;
		case 'm':
			current++;
			*type = TKN_MINUTE;
			break;
		case 's':
			current++;
			*type = TKN_SECOND;
			break;
		default:
			*type = TKN_NUMBER;
		}

		goto FINISH;
	}

	if (!isalpha(c) && c != '_') {
		*type = TKN_ERR;
		goto FINISH;
	}

	while (!ENDED() && (isalnum(CURRENT()) || CURRENT() == '_'))
		(void) NEXT();

	*type = TKN_IDENTIFIER;

FINISH:
	return current;

#undef READ
#undef ENDED
#undef PREV
#undef CURRENT
#undef NEXT
}

enum jy_tkn jry_keyword(const char *ident, uint32_t length)
{
#define KEYWORD(__base, __target, __len)                                       \
	length - ((__base) - ident) == (__len)                                 \
		&& memcmp((__base), (__target), (__len)) == 0

	switch (ident[0]) {
	case 'a':
		if (ident[1] == 's')
			return TKN_ALIAS;
		else if (KEYWORD(ident + 1, "ll", 2))
			return TKN_ALL;
		else if (KEYWORD(ident + 1, "nd", 2))
			return TKN_AND;
		else if (KEYWORD(ident + 1, "ny", 2))
			return TKN_ANY;
		else if (KEYWORD(ident + 1, "ction", 5))
			return TKN_JUMP;

		break;
	case 'b':
		if (KEYWORD(ident + 1, "ool", 3))
			return TKN_BOOL_TYPE;
		else if (KEYWORD(ident + 1, "etween", 6))
			return TKN_BETWEEN;

		break;
	case 'c':
		if (KEYWORD(ident + 1, "ondition", 8))
			return TKN_CONDITION;

		break;
	case 'e':
		if (KEYWORD(ident + 1, "xact", 4))
			return TKN_EXACT;
		else if (KEYWORD(ident + 1, "qual", 4))
			return TKN_EQUAL;
		else if (KEYWORD(ident + 1, "lse", 3))
			return TKN_RESERVED;
		else if (KEYWORD(ident + 1, "lif", 3))
			return TKN_RESERVED;

		break;
	case 'f':
		if (KEYWORD(ident + 1, "alse", 4))
			return TKN_FALSE;
		// fi
		else if (KEYWORD(ident + 1, "i", 1))
			return TKN_RESERVED;
		else if (KEYWORD(ident + 1, "ield", 4))
			return TKN_FIELD;

		break;
	case 'g':
		// gt
		if (KEYWORD(ident + 1, "t", 1))
			return TKN_RESERVED;
		// gte
		else if (KEYWORD(ident + 1, "te", 2))
			return TKN_RESERVED;

		break;
	case 'l':
		if (KEYWORD(ident + 1, "ong", 3))
			return TKN_LONG_TYPE;
		// lt
		else if (KEYWORD(ident + 1, "t", 1))
			return TKN_RESERVED;
		// lte
		else if (KEYWORD(ident + 1, "te", 2))
			return TKN_RESERVED;

		break;
	case 'o':
		if (ident[1] == 'r')
			return TKN_OR;
		else if (KEYWORD(ident + 1, "utput", 5))
			return TKN_OUTPUT;

		break;
	case 'i':
		if (KEYWORD(ident + 1, "n", 1))
			return TKN_RESERVED;
		else if (KEYWORD(ident + 1, "nclude", 6))
			return TKN_INCLUDE;
		else if (KEYWORD(ident + 1, "ngress", 6))
			return TKN_INGRESS;
		else if (KEYWORD(ident + 1, "nput", 4))
			return TKN_INPUT;
		else if (KEYWORD(ident + 1, "mport", 5))
			return TKN_IMPORT;
		// if
		else if (KEYWORD(ident + 1, "f", 1))
			return TKN_RESERVED;

		break;
	case 'j':
		if (KEYWORD(ident + 1, "oin", 3))
			return TKN_JOINX;

		break;
	case 't':
		if (KEYWORD(ident + 1, "rue", 3))
			return TKN_TRUE;
		// then
		else if (KEYWORD(ident + 1, "hen", 3))
			return TKN_RESERVED;

		break;
	case 'r':
		if (KEYWORD(ident + 1, "ule", 3))
			return TKN_RULE;
		else if (KEYWORD(ident + 1, "egex", 4))
			return TKN_REGEX;
		// range
		else if (KEYWORD(ident + 1, "ange", 4))
			return TKN_RESERVED;

		break;
	case 'm':
		if (KEYWORD(ident + 1, "atch", 4))
			return TKN_MATCH;

		break;
	case 'n':
		if (KEYWORD(ident + 1, "ot", 2))
			return TKN_NOT;

		break;
	case 'w':
		if (KEYWORD(ident + 1, "ithin", 5))
			return TKN_WITHIN;

		break;
	case 's':
		if (KEYWORD(ident + 1, "tring", 5))
			return TKN_STRING_TYPE;

		break;
	}

	return TKN_IDENTIFIER;

#undef KEYWORD
}
