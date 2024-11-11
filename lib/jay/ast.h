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

#ifndef JAYVM_AST_H
#define JAYVM_AST_H

#include <stdint.h>

enum jy_ast {
	AST_NONE = -1,
	AST_ROOT,

	AST_IMPORT_STMT,
	AST_INCLUDE_STMT,

	AST_RULE_DECL,
	AST_INGRESS_DECL,

	AST_JUMP_SECT,
	AST_INPUT_SECT,
	AST_MATCH_SECT,
	AST_CONDITION_SECT,
	AST_OUTPUT_SECT,
	AST_FIELD_SECT,

	AST_LONG_TYPE,
	AST_STR_TYPE,
	AST_BOOL_TYPE,

	AST_QACCESS,
	AST_EACCESS,

	AST_ALIAS,
	AST_EVENT,
	AST_EVENT_MEMBER,
	AST_CALL,

	AST_JOINX,
	AST_EXACT,
	AST_BETWEEN,
	AST_WITHIN,
	AST_EQUAL,
	AST_REGEX,

	AST_REGMATCH,
	AST_EQUALITY,
	AST_LESSER,
	AST_GREATER,

	AST_NOT,
	AST_AND,
	AST_OR,

	AST_CONCAT,
	AST_ADDITION,
	AST_SUBTRACT,
	AST_MULTIPLY,
	AST_DIVIDE,

	AST_NAME,
	AST_PATH,

	AST_REGEXP,
	AST_LONG,
	AST_MINUTE,
	AST_HOUR,
	AST_SECOND,
	AST_STRING,
	AST_FALSE,
	AST_TRUE,
};

struct jy_asts {
	enum jy_ast *types;
	// index to a tkn array
	uint32_t    *tkns;
	// index to ast array
	uint32_t   **child;
	// degree
	uint32_t    *childsz;

	// total ast nodes
	uint32_t size;
};

#endif // JAYVM_AST_H
