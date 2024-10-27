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

		while (!ENDED() && CURRENT() != '"')
			(void) NEXT();

		if (ENDED()) {
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
	case '.':
		if (*current == '.') {
			*type	 = TKN_CONCAT;
			current += 1;
		} else {
			*type = TKN_DOT;
		}

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
				NEXT();
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

#define KEYWORD(__base, __target, __len)                                       \
	current == (__base) + (__len)                                          \
		&& memcmp((__base), (__target), (__len)) == 0

	switch (start[0]) {
	case 'a':
		if (start[1] == 's')
			*type = TKN_ALIAS;
		else if (KEYWORD(start + 1, "ll", 2))
			*type = TKN_ALL;
		else if (KEYWORD(start + 1, "nd", 2))
			*type = TKN_AND;
		else if (KEYWORD(start + 1, "ny", 2))
			*type = TKN_ANY;
		else if (KEYWORD(start + 1, "ction", 5))
			*type = TKN_JUMP;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'b':
		if (KEYWORD(start + 1, "ool", 3))
			*type = TKN_BOOL_TYPE;
		else if (KEYWORD(start + 1, "etween", 6))
			*type = TKN_BETWEEN;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'c':
		if (KEYWORD(start + 1, "ondition", 8))
			*type = TKN_CONDITION;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'e':
		if (KEYWORD(start + 1, "xact", 4))
			*type = TKN_EXACT;
		else if (KEYWORD(start + 1, "qual", 4))
			*type = TKN_EQUAL;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'f':
		if (KEYWORD(start + 1, "alse", 4))
			*type = TKN_FALSE;
		else if (KEYWORD(start + 1, "ield", 4))
			*type = TKN_FIELD;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'l':
		if (KEYWORD(start + 1, "ong", 3))
			*type = TKN_LONG_TYPE;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'o':
		if (start[1] == 'r')
			*type = TKN_OR;
		else if (KEYWORD(start + 1, "utput", 5))
			*type = TKN_OUTPUT;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'i':
		if (KEYWORD(start + 1, "nclude", 6))
			*type = TKN_INCLUDE;
		else if (KEYWORD(start + 1, "ngress", 6))
			*type = TKN_INGRESS;
		else if (KEYWORD(start + 1, "nput", 4))
			*type = TKN_INPUT;
		else if (KEYWORD(start + 1, "mport", 5))
			*type = TKN_IMPORT;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'j':
		if (KEYWORD(start + 1, "oin", 3))
			*type = TKN_JOINX;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 't':
		if (KEYWORD(start + 1, "rue", 3))
			*type = TKN_TRUE;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'r':
		if (KEYWORD(start + 1, "ule", 3))
			*type = TKN_RULE;
		else if (KEYWORD(start + 1, "egex", 4))
			*type = TKN_REGEX;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'm':
		if (KEYWORD(start + 1, "atch", 4))
			*type = TKN_MATCH;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'n':
		if (KEYWORD(start + 1, "ot", 2))
			*type = TKN_NOT;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 'w':
		if (KEYWORD(start + 1, "ithin", 5))
			*type = TKN_WITHIN;
		else
			goto IDENTIFIER;

		goto FINISH;
	case 's':
		if (KEYWORD(start + 1, "tring", 5))
			*type = TKN_STRING_TYPE;
		else
			goto IDENTIFIER;

		goto FINISH;
	}

IDENTIFIER:
	*type = TKN_IDENTIFIER;

FINISH:
	return current;
}
