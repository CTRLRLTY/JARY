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

#include <gtest/gtest.h>

extern "C" {
#include "jay/scanner.h"
}

TEST(ScannerTest, ScanEOF)
{
	char str[] = "\0\0";

	enum jy_tkn type;
	uint32_t    len	  = sizeof(str);
	const char *start = str, *end = NULL;
	end = jry_scan(str, len, &type);

	ASSERT_EQ(type, TKN_EOF);
	ASSERT_EQ(1, end - start);
}

TEST(ScannerTest, ScanNewline)
{
	char str[] = "\n\n\n\n\n";

	enum jy_tkn type;
	uint32_t    len	  = sizeof(str);
	const char *start = str, *end = NULL;
	end = jry_scan(str, len, &type);

	ASSERT_EQ(type, TKN_NEWLINE);
	ASSERT_EQ(memcmp(str, start, end - start), 0);
}

TEST(ScannerTest, ScanWhitespace)
{
	enum jy_tkn type;
	char	    str[] = "    \t\t\r\t  \r";
	int32_t	    len	  = sizeof(str);
	const char *start = str, *end = NULL;
	end = jry_scan(str, len, &type);

	ASSERT_EQ(type, TKN_SPACES);
	ASSERT_EQ(memcmp(str, start, end - start), 0);
}

TEST(ScannerTest, ScanSymbol)
{
	char str[] = "()[]{}==~<>:.,$^|\\?\\t\\r\\n\\f";

	enum jy_tkn types[] = {
		TKN_LEFT_PAREN,	  TKN_RIGHT_PAREN,
		TKN_LEFT_BRACKET, TKN_RIGHT_BRACKET,
		TKN_LEFT_BRACE,	  TKN_RIGHT_BRACE,
		TKN_EQ,		  TKN_TILDE,
		TKN_LESSTHAN,	  TKN_GREATERTHAN,
		TKN_COLON,	  TKN_DOT,
		TKN_COMMA,	  TKN_DOLLAR,
		TKN_CARET,	  TKN_VERTBAR,
		TKN_BACKSLASH,	  TKN_QMARK,
		TKN_HT,		  TKN_CR,
		TKN_LF,		  TKN_FF,
	};

	uint32_t length = sizeof(types) / sizeof(types[0]);

	const char *start = str;
	for (uint32_t i = 0; i < length; ++i) {
		enum jy_tkn type;
		start = jry_scan(start, sizeof(str), &type);

		ASSERT_EQ(type, types[i]) << "i: " << i << " lexeme: " << start;
	}
}

TEST(ScannerTest, ScanKeyword)
{
	struct {
		const char *lex;
		enum jy_tkn type;
	} keyword[] = {
		{ "all", TKN_ALL },	    { "as", TKN_ALIAS },
		{ "and", TKN_AND },	    { "any", TKN_ANY },
		{ "false", TKN_FALSE },	    { "true", TKN_TRUE },
		{ "or", TKN_OR },	    { "not", TKN_NOT },
		{ "input", TKN_INPUT },	    { "rule", TKN_RULE },
		{ "import", TKN_IMPORT },   { "ingress", TKN_INGRESS },
		{ "include", TKN_INCLUDE }, { "match", TKN_MATCH },
		{ "action", TKN_JUMP },	    { "condition", TKN_CONDITION },
		{ "output", TKN_OUTPUT },   { "field", TKN_FIELD },
		{ "between", TKN_BETWEEN }, { "join", TKN_JOINX },
		{ "exact", TKN_EXACT },	    { "equal", TKN_EQUAL },
		{ "regex", TKN_REGEX },	    { "string", TKN_STRING_TYPE },
		{ "long", TKN_LONG_TYPE },  { "bool", TKN_BOOL_TYPE },
		{ "if", TKN_RESERVED },	    { "else", TKN_RESERVED },
		{ "elif", TKN_RESERVED },   { "fi", TKN_RESERVED },
		{ "then", TKN_RESERVED },   { "range", TKN_RESERVED },
		{ "gt", TKN_RESERVED },	    { "lt", TKN_RESERVED },
		{ "gte", TKN_RESERVED },    { "lte", TKN_RESERVED },
		{ "in", TKN_RESERVED },
	};

	int keywordsz = sizeof(keyword) / sizeof(keyword[0]);

	for (int i = 0; i < keywordsz; ++i) {
		enum jy_tkn type;
		const char *start = keyword[i].lex;
		// + 1 include '\0'
		size_t	    strsz = strlen(keyword[i].lex) + 1;
		const char *end	  = jry_scan(start, strsz, &type);
		uint32_t    read  = end - start;

		type = jry_keyword(start, read);

		ASSERT_EQ(type, keyword[i].type) << keyword[i].lex;
		ASSERT_EQ(memcmp(keyword[i].lex, start, end - start), 0);
		ASSERT_EQ(strsz - 1, end - start) << "i: " << i;
	}
}

TEST(ScannerTest, ScanString)
{
	enum jy_tkn type;
	char	    str[] = "\"hello world\"";
	const char *start = str, *end = NULL;

	end = jry_scan(start, sizeof(str), &type);
	ASSERT_EQ(type, TKN_STRING);
	ASSERT_EQ(strlen(start), end - start);
	ASSERT_EQ(memcmp(str, start, end - start), 0);
}

TEST(ScannerTest, ScanRegexp)
{
	char	    str[] = "/Hello world\\//";
	enum jy_tkn type;
	const char *start = str, *end = NULL;
	end = jry_scan(start, sizeof(str), &type);

	ASSERT_EQ(type, TKN_REGEXP);
	ASSERT_EQ(strlen(str), end - start);
	ASSERT_EQ(memcmp(str, start, end - start), 0);
}

TEST(ScannerTest, ScanIdentifier)
{
	const char *names[] = {
		"hello",
		"_my_name"
		"identifier123",
	};

	for (uint32_t i = 0; i < sizeof(names) / sizeof(names[0]) - 1; ++i) {
		enum jy_tkn type;
		const char *start = names[i], *end = NULL;
		// + 1 to include '\0'
		uint32_t    len = strlen(start) + 1;
		end		= jry_scan(start, len, &type);
		ASSERT_EQ(type, TKN_IDENTIFIER);
		ASSERT_EQ(memcmp(start, start, end - start), 0);
		ASSERT_EQ(len - 1, end - start);
	}
}
