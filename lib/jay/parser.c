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

#include "parser.h"

#include "ast.h"
#include "error.h"
#include "scanner.h"

#include "jary/common.h"
#include "jary/memory.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CASE_TKN_SECT                                                          \
TKN_INPUT:                                                                     \
	case TKN_MATCH:                                                        \
	case TKN_JUMP:                                                         \
	case TKN_CONDITION:                                                    \
	case TKN_FIELD:                                                        \
	case TKN_OUTPUT

#define CASE_TKN_DECL                                                          \
TKN_RULE:                                                                      \
	case TKN_IMPORT:                                                       \
	case TKN_INGRESS:                                                      \
	case TKN_INCLUDE

struct parser {
	const char *src;
	// currect section type
	enum jy_tkn section;
	uint32_t    srcsz;
	// current tkn id
	uint32_t    tkn;
};

// high to low
enum prec {
	PREC_NONE,
	PREC_ASSIGNMENT,
	PREC_OR,	 // or
	PREC_AND,	 // and
	PREC_EQUALITY,	 // ==
	PREC_COMPARISON, // < > <= >=
	PREC_TERM,	 // + - ..
	PREC_FACTOR,	 // * /
	PREC_UNARY,	 // not -
	PREC_CALL,	 // . () ~
};

static inline enum prec tkn_prec(enum jy_tkn type);

static bool _precedence(struct parser	*p,
			struct jy_asts	*asts,
			struct jy_tkns	*tkns,
			struct tkn_errs *errs,
			uint32_t	*topast,
			enum prec	 rbp);

static inline __use_result int push_ast(struct jy_asts *asts,
					enum jy_ast	type,
					uint32_t	tkn,
					uint32_t       *id)
{
	jry_mem_push(asts->types, asts->size, type);
	jry_mem_push(asts->tkns, asts->size, tkn);
	jry_mem_push(asts->child, asts->size, NULL);
	jry_mem_push(asts->childsz, asts->size, 0);

	if (asts->childsz == NULL)
		return -1;

	if (id != NULL)
		*id = asts->size;

	asts->size += 1;

	return 0;
}

static inline void pop_ast(struct jy_asts *asts)
{
	assert(asts->size > 0);

	uint32_t   id	 = asts->size - 1;
	uint32_t **child = &asts->child[id];

	jry_free(*child);

	*child = NULL;

	asts->size -= 1;
}

static inline __use_result int push_child(struct jy_asts *asts,
					  uint32_t	  astid,
					  uint32_t	  childid)
{
	assert(asts->size > astid);
	assert(asts->size > childid);

	uint32_t **child   = &asts->child[astid];
	uint32_t  *childsz = &asts->childsz[astid];

	if (*childsz == 0) {
		*child = jry_alloc(sizeof(**child));

		if (*child == NULL)
			goto OUT_OF_MEMORY;

	} else {
		uint32_t degree = *childsz + 1;
		*child		= jry_realloc(*child, sizeof(**child) * degree);

		if (*child == NULL)
			goto OUT_OF_MEMORY;
	}

	(*child)[*childsz]  = childid;
	*childsz	   += 1;

	return 0;

OUT_OF_MEMORY:
	return -1;
}

static inline __use_result int push_tkn(struct jy_tkns *tkns,
					enum jy_tkn	type,
					uint32_t	line,
					uint32_t	ofs,
					char	       *lexeme,
					uint32_t	lexsz)
{
	jry_mem_push(tkns->types, tkns->size, type);
	jry_mem_push(tkns->lines, tkns->size, line);
	jry_mem_push(tkns->ofs, tkns->size, ofs);
	jry_mem_push(tkns->lexemes, tkns->size, lexeme);
	jry_mem_push(tkns->lexsz, tkns->size, lexsz);

	if (tkns->lexsz == NULL)
		return -1;

	tkns->size += 1;

	return 0;
}

static inline bool ended(const enum jy_tkn *types, uint32_t length)
{
	return types[length - 1] == TKN_EOF;
}

// advance scanner and fill tkn
static inline void next(struct jy_tkns *tkns,
			const char    **src,
			uint32_t       *srcsz,
			uint32_t       *tkn)
{
	uint32_t    lastkn = tkns->size - 1;
	uint32_t    line   = tkns->lines[lastkn];
	uint32_t    ofs	   = tkns->ofs[lastkn] + tkns->lexsz[lastkn];
	enum jy_tkn type   = tkns->types[lastkn];

	if (type == TKN_NEWLINE || type == TKN_NONE) {
		line += 1;
		ofs   = 1;
	}

	const char *start = *src;
	const char *end	  = jry_scan(start, *srcsz, &type);
	uint32_t    read  = end - start;

	*src   = end;
	*srcsz = (*srcsz > read) ? *srcsz - read : 0;

	uint32_t lexsz = read;

	switch (type) {
	case TKN_IDENTIFIER:
		type = jry_keyword(start, lexsz);
		break;
	case TKN_NEWLINE:
		line += read - 1;
		break;
	default:
		break;
	}

	char *lex = jry_alloc(lexsz + 1);

	// you are screwed.
	if (lex == NULL)
		goto PANIC;

	memcpy(lex, start, lexsz);

	lex[lexsz] = '\0';
	*tkn	   = tkns->size;

	if (push_tkn(tkns, type, line, ofs, lex, lexsz) != 0)
		goto PANIC;

	switch (type) {
	case TKN_SPACES:
		next(tkns, src, srcsz, tkn);
		break;
	case TKN_COMMENT:
		next(tkns, src, srcsz, tkn);
		break;
	default:
		break;
	}

	return;

PANIC:
	jry_free(lex);
	*tkn			    = tkns->size - 1;
	tkns->types[tkns->size - 1] = TKN_EOF;
	return;
}

static inline bool synclist(struct jy_tkns *tkns,
			    const char	  **src,
			    uint32_t	   *srcsz,
			    uint32_t	   *tkn)
{
	enum jy_tkn t = tkns->types[*tkn];

	for (;;) {
		switch (t) {
		case CASE_TKN_DECL:
		case CASE_TKN_SECT:
		case TKN_RIGHT_BRACE:
		case TKN_NEWLINE:
			goto SYNCED;
		case TKN_EOF:
			goto PANIC;
		default:
			next(tkns, src, srcsz, tkn);
			t = tkns->types[*tkn];
		}
	}

SYNCED:
	return false;
PANIC:
	return true;
}

static inline bool syncsection(struct jy_tkns *tkns,
			       const char    **src,
			       uint32_t	      *srcsz,
			       uint32_t	      *tkn)
{
	enum jy_tkn t = tkns->types[*tkn];

	for (;;) {
		switch (t) {
		case CASE_TKN_DECL:
		case CASE_TKN_SECT:
		case TKN_RIGHT_BRACE:
			goto SYNCED;

		case TKN_EOF:
			goto PANIC;
		default:
			break;
		}

		next(tkns, src, srcsz, tkn);
		t = tkns->types[*tkn];
	}

SYNCED:
	return false;
PANIC:
	return true;
}

static bool syncdecl(struct jy_tkns *tkns,
		     const char	   **src,
		     uint32_t	    *srcsz,
		     uint32_t	    *tkn)
{
	enum jy_tkn t = tkns->types[*tkn];

	for (;;) {
		switch (t) {
		case CASE_TKN_DECL:
			goto SYNCED;
		case TKN_EOF:
			goto PANIC;
		default:
			next(tkns, src, srcsz, tkn);
			t = tkns->types[*tkn];
		}
	}

SYNCED:
	return false;
PANIC:
	return true;
}

static inline bool _expr(struct parser	 *p,
			 struct jy_asts	 *asts,
			 struct jy_tkns	 *tkns,
			 struct tkn_errs *errs,
			 uint32_t	 *root)
{
	uint32_t lastast = asts->size - 1;

	if (_precedence(p, asts, tkns, errs, root, PREC_ASSIGNMENT)) {
		// clean until last valid (non inclusive)
		while (lastast + 1 < asts->size)
			pop_ast(asts);

		goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static inline bool _call(struct parser	 *p,
			 struct jy_asts	 *asts,
			 struct jy_tkns	 *tkns,
			 struct tkn_errs *errs,
			 uint32_t	 *root)
{
	uint32_t    left    = *root;
	enum jy_ast type    = asts->types[left];
	uint32_t    nametkn = p->tkn;

	if (type != AST_EACCESS) {
		tkn_error(errs, "invalid invocation", p->tkn, p->tkn);
		goto PANIC;
	}

	// set root to (
	if (push_ast(asts, AST_CALL, p->tkn, root) != 0)
		goto PANIC;

	if (push_child(asts, *root, left) != 0)
		goto PANIC;

	// consume (
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn param = tkns->types[p->tkn];

	while (param != TKN_RIGHT_PAREN) {
		uint32_t paramsz = asts->childsz[*root] - 1;
		uint32_t topast	 = *root;

		// > 2 bytes
		if ((paramsz + 1) & 0x10000) {
			tkn_error(errs, "too many arguments", nametkn, p->tkn);
			goto PANIC;
		}

		if (_expr(p, asts, tkns, errs, &topast))
			goto PANIC;

		if (push_child(asts, *root, topast) != 0)
			goto PANIC;

		if (tkns->types[p->tkn] != TKN_COMMA)
			break;

		next(tkns, &p->src, &p->srcsz, &p->tkn);

		while (tkns->types[p->tkn] == TKN_NEWLINE)
			next(tkns, &p->src, &p->srcsz, &p->tkn);

		param = tkns->types[p->tkn];
	}

	if (tkns->types[p->tkn] != TKN_RIGHT_PAREN) {
		tkn_error(errs, "missing ')'", nametkn, p->tkn);
		goto PANIC;
	}

	// consume ')'
	next(tkns, &p->src, &p->srcsz, &p->tkn);
	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _dot(struct parser	 *p,
		 struct jy_asts	 *asts,
		 struct jy_tkns	 *tkns,
		 struct tkn_errs *errs,
		 uint32_t	 *root)
{
	uint32_t    right;
	uint32_t    left  = *root;
	enum jy_ast rtype = asts->types[left];

	switch (asts->types[left]) {
	case AST_NAME:
	case AST_EVENT:
		break;
	default: {
		tkn_error(errs, "invalid accessor", p->tkn, p->tkn);
		goto PANIC;
	}
	}

	enum jy_ast access;

	switch (p->section) {
	case TKN_CONDITION:
	case TKN_OUTPUT:
	case TKN_JUMP:
		access = AST_EACCESS;
		break;
	case TKN_MATCH:
		access = AST_QACCESS;
		break;
	default:
		goto PANIC;
	}

	// set root to .
	if (push_ast(asts, access, p->tkn, root) != 0)
		goto PANIC;

	// consume .
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		tkn_error(errs, "not an identifier", asts->tkns[*root], p->tkn);
		goto PANIC;
	}

	if (push_ast(asts, rtype, p->tkn, &right) != 0)
		goto PANIC;

	if (push_child(asts, *root, left) != 0)
		goto PANIC;

	if (push_child(asts, *root, right) != 0)
		goto PANIC;

	// consume identifier
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	type = tkns->types[p->tkn];

	switch (type) {
	case TKN_LEFT_PAREN:
		if (_call(p, asts, tkns, errs, root))
			goto PANIC;
	default:
		break;
	}

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _types(struct parser   *p,
		   struct jy_asts  *asts,
		   struct jy_tkns  *tkns,
		   struct tkn_errs *errs,
		   uint32_t	   *root)
{
	uint32_t    sectast = asts->size - 1;
	uint32_t    sectkn  = asts->tkns[sectast];
	uint32_t    nametkn = p->tkn;
	enum jy_tkn type    = tkns->types[nametkn];

	if (type != TKN_IDENTIFIER) {
		tkn_error(errs, "invalid identifier", sectkn, nametkn);
		goto PANIC;
	}

	if (push_ast(asts, AST_EVENT_MEMBER, nametkn, root) != 0)
		goto PANIC;

	// consume name
	next(tkns, &p->src, &p->srcsz, &p->tkn);
	type = tkns->types[p->tkn];

	uint32_t    ntype;
	enum jy_ast name_type;
	switch (type) {
	case TKN_LONG_TYPE:
		name_type = AST_LONG_TYPE;
		break;
	case TKN_STRING_TYPE:
		name_type = AST_STR_TYPE;
		break;
	case TKN_BOOL_TYPE:
		name_type = AST_BOOL_TYPE;
		break;
	default:
		tkn_error(errs, "not a type", sectkn, p->tkn);
		goto PANIC;
	}

	if (push_ast(asts, name_type, p->tkn, &ntype) != 0)
		goto PANIC;

	if (push_child(asts, *root, ntype) != 0)
		goto PANIC;

	// consume type
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return false;

PANIC:
	// clean until last valid (non inclusive)
	while (sectast + 1 < asts->size)
		pop_ast(asts);

	return true;
}

static bool _section(struct parser   *p,
		     struct jy_asts  *asts,
		     struct jy_tkns  *tkns,
		     struct tkn_errs *errs,
		     uint32_t	      declast)
{
	bool ret	     = false;
	enum jy_ast decltype = asts->types[declast];
	uint32_t sectast     = asts->size - 1;
	uint32_t secttkn     = p->tkn;
	p->section	     = tkns->types[secttkn];

	// consume section
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	bool (*listfn)(struct parser *, struct jy_asts *, struct jy_tkns *,
		       struct tkn_errs *, uint32_t *);

	listfn = NULL;

	switch (p->section) {
	case TKN_JUMP:
		if (decltype != AST_RULE_DECL)
			goto INVALID_SECTION;

		asts->types[sectast] = AST_JUMP_SECT;
		listfn		     = _expr;
		break;
	case TKN_INPUT:
		if (decltype != AST_INGRESS_DECL)
			goto INVALID_SECTION;

		asts->types[sectast] = AST_INPUT_SECT;
		goto INVALID_SECTION;
		break;
	case TKN_MATCH:
		if (decltype != AST_RULE_DECL)
			goto INVALID_SECTION;

		asts->types[sectast] = AST_MATCH_SECT;
		listfn		     = _expr;
		break;
	case TKN_CONDITION:
		if (decltype != AST_RULE_DECL)
			goto INVALID_SECTION;

		asts->types[sectast] = AST_CONDITION_SECT;
		listfn		     = _expr;
		break;
	case TKN_OUTPUT:
		if (decltype != AST_RULE_DECL)
			goto INVALID_SECTION;
		asts->types[sectast] = AST_OUTPUT_SECT;
		listfn		     = _expr;
		break;
	case TKN_FIELD:
		if (decltype != AST_INGRESS_DECL)
			goto INVALID_SECTION;

		asts->types[sectast] = AST_FIELD_SECT;
		listfn		     = _types;
		break;
	default:
		goto INVALID_SECTION;
	}

	if (tkns->types[p->tkn] != TKN_COLON) {
		tkn_error(errs, "missing ':'", secttkn, p->tkn);
		goto PANIC;
	}

	// consume colon
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	while (tkns->types[p->tkn] == TKN_NEWLINE)
		next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn listype = tkns->types[p->tkn];

	for (;;) {
		switch (listype) {
		case CASE_TKN_DECL:
		case CASE_TKN_SECT:
		case TKN_RIGHT_BRACE:
			goto FINISH;
		case TKN_EOF:
			goto PANIC;
		default:
			break;
		}

		uint32_t root  = sectast;
		bool	 panic = false;

		if (listfn(p, asts, tkns, errs, &root))
			panic = synclist(tkns, &p->src, &p->srcsz, &p->tkn);
		else
			panic = push_child(asts, sectast, root) != 0;

		if (panic)
			goto PANIC;

		if (tkns->types[p->tkn] != TKN_NEWLINE) {
			tkn_error(errs, "missing newline", secttkn, p->tkn);
			goto PANIC;
		}

		// consume NEWLINE
		while (tkns->types[p->tkn] == TKN_NEWLINE)
			next(tkns, &p->src, &p->srcsz, &p->tkn);

		listype = tkns->types[p->tkn];
	}

	goto FINISH;

INVALID_SECTION: {
	tkn_error(errs, "not a section", asts->tkns[declast], p->tkn);
}
PANIC:
	// remove all node up to sectast (inclusive)
	while (sectast < asts->size)
		pop_ast(asts);

	ret = true;
FINISH:
	p->section = TKN_NONE;
	return ret;
}

static inline bool _block(struct parser	  *p,
			  struct jy_asts  *asts,
			  struct jy_tkns  *tkns,
			  struct tkn_errs *errs,
			  uint32_t	   declast)
{
	uint32_t decltkn = asts->tkns[declast];

	if (tkns->types[p->tkn] != TKN_LEFT_BRACE) {
		tkn_error(errs, "missing '{'", decltkn, p->tkn);
		goto PANIC;
	}

	// consume {
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	for (;;) {
		switch (tkns->types[p->tkn]) {
		case CASE_TKN_DECL:
		case TKN_RIGHT_BRACE:
			goto CLOSING;
		case TKN_EOF:
			goto PANIC;
		case TKN_NEWLINE:
			next(tkns, &p->src, &p->srcsz, &p->tkn);
			continue;
		default:
			break;
		}

		uint32_t sectast;

		if (push_ast(asts, AST_NONE, p->tkn, &sectast) != 0)
			goto PANIC;

		if (_section(p, asts, tkns, errs, declast)) {
			if (syncsection(tkns, &p->src, &p->srcsz, &p->tkn))
				goto CLOSING;
			continue;
		}

		if (push_child(asts, declast, sectast) != 0)
			goto PANIC;
	}

CLOSING:
	if (tkns->types[p->tkn] != TKN_RIGHT_BRACE) {
		tkn_error(errs, "missing '}'", decltkn, p->tkn);
		goto PANIC;
	}

	// consume }
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return false;

PANIC:
	// remove all node up to declast (inclusive)
	while (declast < asts->size)
		pop_ast(asts);

	return true;
}

static inline bool _identifier(struct parser   *p,
			       struct jy_asts  *asts,
			       struct jy_tkns  *tkns,
			       struct tkn_errs *errs,
			       uint32_t		ast)
{
	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		tkn_error(errs, "not an identifier", asts->tkns[ast], p->tkn);
		goto PANIC;
	}

	asts->tkns[ast] = p->tkn;
	// consume name
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return false;
PANIC:
	return true;
}

static inline bool _declstmt(struct parser   *p,
			     struct jy_asts  *asts,
			     struct jy_tkns  *tkns,
			     struct tkn_errs *errs)
{
	uint32_t    declast = asts->size - 1;
	uint32_t    decltkn = p->tkn;
	enum jy_tkn dcltype = tkns->types[decltkn];

	// consume decl tkn
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	switch (dcltype) {
	case TKN_IMPORT:
		asts->types[declast] = AST_IMPORT_STMT;
		if (_identifier(p, asts, tkns, errs, declast))
			goto PANIC;

		break;
	case TKN_INGRESS:
		asts->types[declast] = AST_INGRESS_DECL;

		if (_identifier(p, asts, tkns, errs, declast))
			goto PANIC;

		if (_block(p, asts, tkns, errs, declast))
			goto PANIC;

		break;
	case TKN_RULE:
		asts->types[declast] = AST_RULE_DECL;

		if (_identifier(p, asts, tkns, errs, declast))
			goto PANIC;

		if (_block(p, asts, tkns, errs, declast))
			goto PANIC;

		break;
	case TKN_INCLUDE: {
		asts->types[declast] = AST_INCLUDE_STMT;
		uint32_t pattkn	     = p->tkn;

		if (tkns->types[pattkn] != TKN_STRING) {
			tkn_error(errs, "expected a string", decltkn, pattkn);
			goto PANIC;
		}

		// consume path token
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		uint32_t pathast;

		if (push_ast(asts, AST_PATH, pattkn, &pathast) != 0)
			goto PANIC;

		if (push_child(asts, declast, pathast) != 0)
			goto PANIC;

		break;
	}

	default: {
		tkn_error(errs, "not a declaration", decltkn, decltkn);
		goto PANIC;
	}
	}

	return false;
PANIC:
	// remove all node up to declast (inclusive)
	while (declast < asts->size)
		pop_ast(asts);

	return true;
}

static int _entry(struct parser	  *p,
		  struct jy_asts  *asts,
		  struct jy_tkns  *tkns,
		  struct tkn_errs *errs)
{
	uint32_t roottkn = tkns->size;
	int	 status	 = push_tkn(tkns, TKN_NONE, 0, 0, NULL, 0);

	if (status != 0)
		return status;

	uint32_t rootast;

	status = push_ast(asts, AST_ROOT, roottkn, &rootast);

	if (status != 0)
		return status;

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	while (tkns->types[p->tkn] == TKN_NEWLINE)
		next(tkns, &p->src, &p->srcsz, &p->tkn);

	while (!ended(tkns->types, tkns->size)) {
		uint32_t declast;

		status = push_ast(asts, AST_NONE, p->tkn, &declast);

		if (status != 0)
			return status;

		if (_declstmt(p, asts, tkns, errs)) {
			syncdecl(tkns, &p->src, &p->srcsz, &p->tkn);
			continue;
		}

		status = push_child(asts, rootast, declast);

		if (status != 0)
			return status;

		while (tkns->types[p->tkn] == TKN_NEWLINE)
			next(tkns, &p->src, &p->srcsz, &p->tkn);
	}

	return 0;
}

static inline bool _prefix(enum jy_tkn	    type,
			   struct parser   *p,
			   struct jy_asts  *asts,
			   struct jy_tkns  *tkns,
			   struct tkn_errs *errs,
			   uint32_t	   *root)
{
	switch (type) {
	case TKN_ERR:
		tkn_error(errs, "not a token", p->tkn, p->tkn);

		return true;
	case TKN_ERR_STR:
		tkn_error(errs, "unterminated string", p->tkn, p->tkn);

		return true;

	case TKN_LEFT_PAREN: {
		uint32_t grptkn = p->tkn;
		// consume (
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_expr(p, asts, tkns, errs, root))
			goto PANIC;

		if (tkns->types[p->tkn] != TKN_RIGHT_PAREN) {
			tkn_error(errs, "missing ')'", grptkn, p->tkn);
			goto PANIC;
		}

		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);
	}

	case TKN_NOT: {
		if (push_ast(asts, AST_NOT, p->tkn, root) != 0)
			goto PANIC;

		uint32_t topast = 0;

		// consume not
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &topast, PREC_UNARY))
			goto PANIC;

		if (push_child(asts, *root, topast) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}

	case TKN_IDENTIFIER:
		if (push_ast(asts, AST_NAME, p->tkn, root) != 0)
			return true;

		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);

	case TKN_DOLLAR: {
		uint32_t dlrtkn = p->tkn;
		// consume $
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		enum jy_tkn type = tkns->types[p->tkn];

		if (type != TKN_IDENTIFIER) {
			tkn_error(errs, "not an identifier", dlrtkn, p->tkn);
			goto PANIC;
		}

		if (push_ast(asts, AST_EVENT, p->tkn, root) != 0)
			goto PANIC;

		// consume name
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);
	}

	case TKN_STRING:
		if (push_ast(asts, AST_STRING, p->tkn, root) != 0)
			goto PANIC;

		// consume string
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);

	case TKN_NUMBER:
		if (push_ast(asts, AST_LONG, p->tkn, root) != 0)
			goto PANIC;

		// consume number
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);

	case TKN_REGEXP:
		if (push_ast(asts, AST_REGEXP, p->tkn, root) != 0)
			goto PANIC;

		// consume regexp
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);

	case TKN_FALSE:
		if (push_ast(asts, AST_FALSE, p->tkn, root) != 0)
			goto PANIC;

		// consume false
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);

	case TKN_TRUE:
		if (push_ast(asts, AST_TRUE, p->tkn, root) != 0)
			goto PANIC;

		// consume true
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);

	case TKN_HOUR:
		if (push_ast(asts, AST_HOUR, p->tkn, root) != 0)
			goto PANIC;

		// consume hour
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);

	case TKN_MINUTE:
		if (push_ast(asts, AST_MINUTE, p->tkn, root) != 0)
			goto PANIC;

		// consume minute
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);

	case TKN_SECOND:
		if (push_ast(asts, AST_SECOND, p->tkn, root) != 0)
			goto PANIC;

		// consume number
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		return ended(tkns->types, tkns->size);

	default:
PANIC:
		return true;
	}
}

static inline bool _infix(enum jy_tkn	   type,
			  struct parser	  *p,
			  struct jy_asts  *asts,
			  struct jy_tkns  *tkns,
			  struct tkn_errs *errs,
			  uint32_t	  *root)
{
	uint32_t left  = *root;
	uint32_t right = left;
	uint32_t optkn = p->tkn;

	switch (type) {
	case TKN_LEFT_PAREN:
		return _call(p, asts, tkns, errs, root);
	case TKN_DOT:
		return _dot(p, asts, tkns, errs, root);
	case TKN_PLUS: {
		if (push_ast(asts, AST_ADDITION, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_CONCAT: {
		if (push_ast(asts, AST_CONCAT, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_MINUS: {
		if (push_ast(asts, AST_SUBTRACT, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_SLASH: {
		if (push_ast(asts, AST_DIVIDE, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_STAR: {
		if (push_ast(asts, AST_MULTIPLY, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_JOINX: {
		if (push_ast(asts, AST_JOINX, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_EXACT: {
		if (push_ast(asts, AST_EXACT, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_BETWEEN: {
		if (push_ast(asts, AST_BETWEEN, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_WITHIN: {
		if (push_ast(asts, AST_WITHIN, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_REGEX: {
		if (push_ast(asts, AST_REGEX, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_EQUAL: {
		if (push_ast(asts, AST_EQUAL, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_TILDE: {
		if (push_ast(asts, AST_REGMATCH, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_EQ: {
		if (push_ast(asts, AST_EQUALITY, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_LESSTHAN: {
		if (push_ast(asts, AST_LESSER, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_GREATERTHAN: {
		if (push_ast(asts, AST_GREATER, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_AND: {
		if (push_ast(asts, AST_AND, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	case TKN_OR: {
		if (push_ast(asts, AST_OR, optkn, root) != 0)
			goto PANIC;

		// consume binary operator
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (_precedence(p, asts, tkns, errs, &right, tkn_prec(type)))
			goto PANIC;

		if (push_child(asts, *root, left) != 0)
			goto PANIC;

		if (push_child(asts, *root, right) != 0)
			goto PANIC;

		return ended(tkns->types, tkns->size);
	}
	default:
PANIC:
		return true;
	}
}

static inline enum prec tkn_prec(enum jy_tkn type)
{
	switch (type) {
	case TKN_LEFT_PAREN:
	case TKN_DOT:
		return PREC_CALL;
	case TKN_PLUS:
	case TKN_MINUS:
	case TKN_CONCAT:
		return PREC_TERM;
	case TKN_SLASH:
	case TKN_STAR:
		return PREC_FACTOR;
	case TKN_JOINX:
		return PREC_CALL - 1;

	case TKN_EXACT:
	case TKN_BETWEEN:
	case TKN_WITHIN:
	case TKN_REGEX:
	case TKN_EQUAL:
	case TKN_TILDE:
	case TKN_EQ:
		return PREC_EQUALITY;

	case TKN_LESSTHAN:
	case TKN_GREATERTHAN:
		return PREC_COMPARISON;

	case TKN_AND:
		return PREC_AND;
	case TKN_OR:
		return PREC_OR;

	default:
		return PREC_NONE;
	}
}

static bool _precedence(struct parser	*p,
			struct jy_asts	*asts,
			struct jy_tkns	*tkns,
			struct tkn_errs *errs,
			uint32_t	*root,
			enum prec	 rbp)
{
	enum jy_tkn pretype = tkns->types[p->tkn];

	if (_prefix(pretype, p, asts, tkns, errs, root)) {
		tkn_error(errs, "not an expression", asts->tkns[*root], p->tkn);
		goto PANIC;
	}

	enum jy_tkn inftype = tkns->types[p->tkn];
	enum prec   lbp	    = tkn_prec(inftype);

	while (rbp < lbp) {
		if (_infix(inftype, p, asts, tkns, errs, root))
			goto PANIC;

		inftype = tkns->types[p->tkn];
		lbp	= tkn_prec(inftype);
	}

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static inline void free_asts(struct jy_asts *asts)
{
	jry_free(asts->types);
	jry_free(asts->tkns);

	for (uint32_t i = 0; i < asts->size; ++i)
		jry_free(asts->child[i]);

	jry_free(asts->child);
	jry_free(asts->childsz);
}

static inline void free_tkns(struct jy_tkns *tkns)
{
	jry_free(tkns->types);
	jry_free(tkns->lines);
	jry_free(tkns->ofs);

	for (uint32_t i = 0; i < tkns->size; ++i)
		jry_free(tkns->lexemes[i]);

	jry_free(tkns->lexemes);
	jry_free(tkns->lexsz);
}

void jry_parse(struct sc_mem   *alloc,
	       struct jy_asts  *ast,
	       struct jy_tkns  *tkns,
	       struct tkn_errs *errs,
	       const char      *src,
	       uint32_t		length)
{
	struct parser p = {
		.src   = src,
		.srcsz = length,
	};

	if (sc_reap(alloc, ast, (free_t) free_asts))
		return;
	if (sc_reap(alloc, tkns, (free_t) free_tkns))
		return;
	if (sc_reap(alloc, errs, (free_t) free_errs))
		return;

	_entry(&p, ast, tkns, errs);
}
