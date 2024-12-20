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

#ifndef JAYVM_COMPILER_H
#define JAYVM_COMPILER_H

#include "jary/types.h"

#include <stdint.h>

struct sc_mem;
struct jy_asts;
struct jy_tkns;
struct tkn_errs;

enum jy_opcode {
	JY_OP_PUSH8,
	JY_OP_PUSH16,

	JY_OP_SETBF8,

	JY_OP_LOAD,
	JY_OP_JOIN,
	JY_OP_EQUAL,
	JY_OP_JMPT,
	JY_OP_JMPF,
	JY_OP_CALL,

	JY_OP_QUERY,
	JY_OP_REGEX,
	JY_OP_BETWEEN,
	JY_OP_OUTPUT,
	JY_OP_WITHIN,

	JY_OP_NOT,
	JY_OP_CMPSTR,
	JY_OP_CMP,
	JY_OP_LT,
	JY_OP_GT,

	JY_OP_ADD,
	JY_OP_CONCAT,
	JY_OP_SUB,
	JY_OP_MUL,
	JY_OP_DIV,

	JY_OP_END
};

struct jy_jay {
	// global names
	struct jy_defs *names;
	uint8_t	       *codes;
	uint8_t	       *fcodes;

	// rule offset within the main chunk;
	uint32_t       *rulecofs;
	// rule ordinal from the name table;
	uint16_t       *rulenids;
	// constant table
	union jy_value *vals;
	enum jy_ktype  *types;
	uint32_t	codesz;
	uint32_t	fcodesz;
	uint16_t	valsz;
	uint16_t	rulesz;
};

int jry_compile(struct sc_mem	     *alloc,
		struct jy_jay	     *ctx,
		struct tkn_errs	     *errs,
		const char	     *mdir,
		const struct jy_asts *asts,
		const struct jy_tkns *tkns);

#endif // JAYVM_COMPILER_H
