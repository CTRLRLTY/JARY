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
#include "ast.h"
#include "compiler.h"
#include "error.h"
#include "parser.h"
#include "token.h"

#include "jary/memory.h"
#include "jary/types.h"
}

static size_t read_file(const char *path, char **dst)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	uint32_t file_size = ftell(file);
	rewind(file);

	auto   buffer	  = (char *) malloc(file_size + 1);
	size_t bytes_read = fread(buffer, sizeof(char), file_size, file);

	if (bytes_read < file_size) {
		fprintf(stderr, "could not read file \"%s\".\n", path);
		exit(74);
	}

	buffer[bytes_read] = '\0';
	*dst		   = buffer;

	fclose(file);
	return file_size + 1;
}

TEST(CompilerTest, Basic)
{
	char *src = NULL;

	struct jy_asts	asts  = { .tkns = NULL };
	struct jy_tkns	tkns  = { .lexemes = NULL };
	struct tkn_errs errs  = { .msgs = NULL };
	struct jy_jay	ctx   = { .codes = NULL };
	struct sc_mem	alloc = { .buf = NULL };
	size_t		srcsz = read_file(BASIC_JARY_PATH, &src);

	sc_reap(&alloc, src, free);

	const char mdir[] = "../modules/";

	jry_parse(&alloc, &asts, &tkns, &errs, src, srcsz);
	jry_compile(&alloc, &ctx, &errs, mdir, &asts, &tkns);

	// clang-format off
	uint8_t codes[] = {
                //
                JY_OP_PUSH8, 11,
                JY_OP_PUSH8, 8,
                JY_OP_EQUAL,
                JY_OP_PUSH8, 3,
                JY_OP_PUSH8, 9,
                JY_OP_QUERY,
                JY_OP_END,
        };
	// clang-format on

	ASSERT_EQ(errs.size, 0);
	ASSERT_EQ(ctx.codesz, sizeof(codes));

	for (size_t i = 0; i < sizeof(codes); ++i)
		ASSERT_EQ(ctx.codes[i], codes[i]) << "codes[" << i << "]";

	sc_free(&alloc);
}
