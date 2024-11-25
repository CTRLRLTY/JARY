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

#ifndef JAYVM_TOKEN_H
#define JAYVM_TOKEN_H

#include <stdint.h>

enum jy_tkn {
	TKN_NONE = -1,
	TKN_ERR,
	TKN_ERR_STR,

	TKN_LEFT_PAREN,
	TKN_RIGHT_PAREN,
	TKN_LEFT_BRACE,
	TKN_RIGHT_BRACE,
	TKN_LEFT_BRACKET,
	TKN_RIGHT_BRACKET,
	TKN_DOT,
	TKN_COMMA,
	TKN_COLON,
	TKN_NEWLINE,
	TKN_SPACES,

	TKN_CARET,
	TKN_QMARK,
	TKN_VERTBAR,
	TKN_BACKSLASH,

	TKN_HT,
	TKN_FF,
	TKN_CR,
	TKN_LF,

	TKN_RULE,
	TKN_IMPORT,
	TKN_INCLUDE,
	TKN_INGRESS,

	// > SECTIONS
	TKN_JUMP,
	TKN_OUTPUT,
	TKN_INPUT,
	TKN_MATCH,
	TKN_CONDITION,
	TKN_FIELD,
	// < SECTIONS

	TKN_WITHIN,
	TKN_BETWEEN,
	TKN_REGEX,

	TKN_LONG_TYPE,
	TKN_STRING_TYPE,
	TKN_BOOL_TYPE,

	// < OPERATOR SYMBOL
	TKN_TILDE,

	TKN_CONCAT,
	TKN_MINUS,
	TKN_PLUS,
	TKN_STAR,
	TKN_SLASH,
	TKN_COMMENT,

	TKN_JOINX,
	TKN_EXACT,
	TKN_EQUAL,

	TKN_EQ,
	TKN_LESSTHAN,
	TKN_GREATERTHAN,
	TKN_AND,
	TKN_OR,
	TKN_NOT,
	TKN_ANY,
	TKN_ALL,
	// > OPERATOR SYMBOL

	// < LITERAL
	TKN_REGEXP,
	TKN_STRING,
	TKN_NUMBER,
	TKN_FALSE,
	TKN_TRUE,
	TKN_HOUR,
	TKN_MINUTE,
	TKN_SECOND,
	// > LITERAL

	TKN_IDENTIFIER,
	TKN_DOLLAR,
	TKN_ALIAS,

	TKN_CUSTOM,
	TKN_EOF,

	TKN_RESERVED,
};

struct jy_tkns {
	enum jy_tkn *types;
	uint32_t    *lines;
	uint32_t    *ofs;
	char	   **lexemes;
	uint32_t    *lexsz;
	uint32_t     size;
};

#endif // JAYVM_TOKEN_H
