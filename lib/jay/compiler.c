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

#include "compiler.h"

#include "ast.h"
#include "dload.h"
#include "error.h"
#include "token.h"

#include "jary/common.h"
#include "jary/defs.h"
#include "jary/memory.h"
#include "jary/types.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct expr {
	// Good luck tracing this, cuz its FUCKED!
	// This id can be any id.
	uint32_t      id;
	enum jy_ktype type;
};

struct compiler {
	// global names
	struct jy_defs	*names;
	// code chunk array
	uint8_t	       **codes;
	// constant table
	union jy_value **vals;
	enum jy_ktype  **types;
	uint32_t	*codesz;
	uint16_t	*valsz;
};

// expression compile subroutine signature
typedef bool (*cmplfn_t)(const struct jy_asts *,
			 const struct jy_tkns *,
			 uint32_t,
			 struct compiler *,
			 struct tkn_errs *,
			 struct jy_defs *,
			 struct expr *);

static inline cmplfn_t rule_expression(enum jy_ast type);

static inline __use_result int emit_byte(uint8_t   code,
					 uint8_t **codes,
					 uint32_t *codesz)
{
	jry_mem_push(*codes, *codesz, code);

	if (*codes == NULL)
		return -1;

	*codesz += 1;

	return 0;
}

static inline __use_result int emit_push(uint32_t  constant,
					 uint8_t **code,
					 uint32_t *codesz)
{
	int res = 1;

	if (constant <= 0xff) {
		res = emit_byte(JY_OP_PUSH8, code, codesz);
		res = emit_byte(constant, code, codesz);

	} else if (constant <= 0xffff) {
		res = emit_byte(JY_OP_PUSH16, code, codesz);
		res = emit_byte(constant & 0x00FF, code, codesz);
		res = emit_byte(constant & 0xFF00, code, codesz);
	}

	return res;
}

static inline __use_result int emit_cnst(union jy_value	  value,
					 enum jy_ktype	  type,
					 union jy_value **vals,
					 enum jy_ktype	**types,
					 uint16_t	 *length)
{
	jry_mem_push(*vals, *length, value);

	if (*vals == NULL)
		goto OUT_OF_MEMORY;

	jry_mem_push(*types, *length, type);

	if (*types == NULL)
		goto OUT_OF_MEMORY;

	*length += 1;

	return 0;

OUT_OF_MEMORY:
	return -1;
}

static inline bool _expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       id,
			 struct compiler      *ctx,
			 struct tkn_errs      *errs,
			 struct jy_defs	      *scope,
			 struct expr	      *expr)
{
	enum jy_ast type = asts->types[id];
	cmplfn_t    fn	 = rule_expression(type);
	assert(fn != NULL);

	return fn(asts, tkns, id, ctx, errs, scope, expr);
}

static bool _bool_expr(const struct jy_asts *asts,
		       const struct jy_tkns *__unused(tkns),
		       uint32_t		     ast,
		       struct compiler	    *ctx,
		       struct tkn_errs	    *__unused(errs),
		       struct jy_defs	    *__unused(scope),
		       struct expr	    *expr)
{
	enum jy_ast type = asts->types[ast];

	switch (type) {
	case AST_FALSE:
		expr->id = 1;
		break;
	case AST_TRUE:
		expr->id = 2;
		break;
	default:
		goto PANIC;
	}

	if (emit_push(expr->id, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	expr->type = JY_K_BOOL;
	return false;

PANIC:
	return true;
}

static bool _time_expr(const struct jy_asts *asts,
		       const struct jy_tkns *tkns,
		       uint32_t		     ast,
		       struct compiler	    *ctx,
		       struct tkn_errs	    *__unused(errs),
		       struct jy_defs	    *__unused(scope),
		       struct expr	    *expr)
{
	uint32_t tkn	= asts->tkns[ast];
	char	*lexeme = tkns->lexemes[tkn];

	struct jy_time_ofs timeofs = { .offset = strtol(lexeme, NULL, 10) };

	switch (asts->types[ast]) {
	case AST_HOUR:
		timeofs.time = JY_TIME_HOUR;
		break;
	case AST_MINUTE:
		timeofs.time = JY_TIME_MINUTE;
		break;
	case AST_SECOND:
		timeofs.time = JY_TIME_SECOND;
		break;
	default:
		goto PANIC;
	}

	union jy_value view = { .timeofs = timeofs };

	union jy_value *vals  = *ctx->vals;
	size_t		valsz = *ctx->valsz;
	enum jy_ktype  *types = *ctx->types;

	for (uint32_t i = 0; i < valsz; ++i) {
		enum jy_ktype	   type = types[i];
		struct jy_time_ofs tofs = vals[i].timeofs;

		if (type != JY_K_TIME)
			continue;

		if (memcmp(&timeofs, &tofs, sizeof(tofs)))
			continue;

		expr->id = i;

		goto EMIT;
	}

	expr->id = valsz;

	if (emit_cnst(view, JY_K_TIME, ctx->vals, ctx->types, ctx->valsz))
		goto PANIC;

EMIT:
	expr->type = JY_K_TIME;

	if (emit_push(expr->id, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	return false;

PANIC:
	return true;
}

static bool _long_expr(const struct jy_asts *asts,
		       const struct jy_tkns *tkns,
		       uint32_t		     id,
		       struct compiler	    *ctx,
		       struct tkn_errs	    *__unused(errs),
		       struct jy_defs	    *__unused(scope),
		       struct expr	    *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];

	union jy_value num = { .i64 = strtol(lexeme, NULL, 10) };
	expr->type	   = JY_K_LONG;

	size_t		valsz = *ctx->valsz;
	union jy_value *vals  = *ctx->vals;
	enum jy_ktype  *types = *ctx->types;

	for (uint32_t i = 0; i < valsz; ++i) {
		enum jy_ktype t = types[i];
		long	      v = vals[i].i64;

		if (t != JY_K_LONG || v != num.i64)
			continue;

		expr->id = i;

		goto DONE;
	}

	expr->id = valsz;

	if (emit_cnst(num, JY_K_LONG, ctx->vals, ctx->types, ctx->valsz) != 0)
		goto PANIC;

DONE:
	if (emit_push(expr->id, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	return false;

PANIC:
	return true;
}

static bool _string_expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       id,
			 struct compiler      *ctx,
			 struct tkn_errs      *__unused(errs),
			 struct jy_defs	      *__unused(scope),
			 struct expr	      *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];
	uint32_t lexsz	= tkns->lexsz[tkn];

	size_t		valsz = *ctx->valsz;
	union jy_value *vals  = *ctx->vals;
	enum jy_ktype  *types = *ctx->types;

	for (uint32_t i = 0; i < valsz; ++i) {
		if (types[i] != JY_K_STR)
			continue;

		struct jy_str *v = vals[i].str;

		if (v->size != lexsz || memcmp(v->cstr, lexeme, lexsz))
			continue;

		expr->id = i;

		goto EMIT;
	}

	uint32_t       allocsz = sizeof(struct jy_str) + lexsz + 1;
	union jy_value value   = { .str = jry_alloc(allocsz) };

	if (value.str == NULL)
		goto PANIC;

	value.str->size = lexsz;
	memcpy(value.str->cstr, lexeme, lexsz);
	value.str->cstr[lexsz] = '\0';
	expr->id	       = valsz;

	if (emit_cnst(value, JY_K_STR, ctx->vals, ctx->types, ctx->valsz))
		goto PANIC;
EMIT:
	if (emit_push(expr->id, ctx->codes, ctx->codesz))
		goto PANIC;

	expr->type = JY_K_STR;

	return false;
PANIC:
	return true;
}

static bool _regexp_expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       id,
			 struct compiler      *ctx,
			 struct tkn_errs      *errs,
			 struct jy_defs	      *scope,
			 struct expr	      *expr)
{
	if (_string_expr(asts, tkns, id, ctx, errs, scope, expr))
		return true;

	expr->type = JY_K_REGEX;

	return false;
}

static bool _descriptor_expr(const struct jy_asts *asts,
			     const struct jy_tkns *tkns,
			     uint32_t		   id,
			     struct compiler	  *ctx,
			     struct tkn_errs	  *errs,
			     struct jy_defs	  *scope,
			     struct expr	  *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];

	uint32_t nid;

	if (!def_find(scope, lexeme, &nid)) {
		tkn_error(errs, "missing definition", tkn, tkn);
		goto PANIC;
	}

	union jy_value	value;
	size_t		valsz = *ctx->valsz;
	union jy_value *vals  = *ctx->vals;
	enum jy_ktype  *types = *ctx->types;

	uint32_t val_k_id   = valsz;
	uint32_t scope_k_id = -1u;

	for (uint32_t i = 0; i < valsz; ++i) {
		switch (types[i]) {
		case JY_K_MODULE:
		case JY_K_EVENT:
			break;
		default:
			continue;
		}

		if (scope == vals[i].def) {
			scope_k_id = i;
			break;
		}
	}

	assert(scope_k_id != -1u && "scope not in constant table");

	value.dscptr.name   = scope_k_id;
	value.dscptr.member = nid;

	for (uint32_t i = 0; i < valsz; ++i) {
		if (JY_K_DESCRIPTOR != types[i])
			continue;

		if (value.i64 == vals[i].i64) {
			val_k_id = i;
			goto EMIT;
		}
	}

	if (emit_cnst(value, JY_K_DESCRIPTOR, ctx->vals, ctx->types,
		      ctx->valsz))
		goto PANIC;
EMIT:
	if (emit_push(val_k_id, ctx->codes, ctx->codesz))
		goto PANIC;

	expr->id   = val_k_id;
	expr->type = scope->types[nid];

	return false;
PANIC:
	return true;
}

static bool _qaccess_expr(const struct jy_asts *asts,
			  const struct jy_tkns *tkns,
			  uint32_t		id,
			  struct compiler      *ctx,
			  struct tkn_errs      *errs,
			  struct jy_defs       *scope,
			  struct expr	       *expr)
{
	assert(asts->childsz[id] == 2);

	uint32_t left  = asts->child[id][0];
	uint32_t right = asts->child[id][1];

	uint32_t oldsz = *ctx->codesz;

	if (_expr(asts, tkns, left, ctx, errs, scope, expr))
		return true;

	// discard previous codes
	union jy_value *vals   = *ctx->vals;
	*ctx->codesz	       = oldsz;
	struct jy_desc	desc   = vals[expr->id].dscptr;
	struct jy_defs *lscope = vals[desc.name].def;
	lscope		       = lscope->vals[desc.member].def;

	if (_expr(asts, tkns, right, ctx, errs, lscope, expr))
		return true;

	return false;
}

static bool _eaccess_expr(const struct jy_asts *asts,
			  const struct jy_tkns *tkns,
			  uint32_t		id,
			  struct compiler      *ctx,
			  struct tkn_errs      *errs,
			  struct jy_defs       *scope,
			  struct expr	       *expr)
{
	if (_qaccess_expr(asts, tkns, id, ctx, errs, scope, expr))
		return true;

	if (emit_byte(JY_OP_LOAD, ctx->codes, ctx->codesz))
		return true;

	return false;
}

static bool _call_expr(const struct jy_asts *asts,
		       const struct jy_tkns *tkns,
		       uint32_t		     ast,
		       struct compiler	    *ctx,
		       struct tkn_errs	    *errs,
		       struct jy_defs	    *scope,
		       struct expr	    *expr)
{
	uint32_t name = asts->child[ast][0];
	uint32_t tkn  = asts->tkns[name];

	if (_expr(asts, tkns, name, ctx, errs, scope, expr))
		goto PANIC;

	enum jy_ktype type = expr->type;

	if (type != JY_K_FUNC) {
		tkn_error(errs, "type mismatch", tkn, tkn);
		goto PANIC;
	}

	union jy_value *vals = *ctx->vals;

	union jy_value	desc  = vals[expr->id];
	union jy_value	def   = vals[desc.dscptr.name];
	union jy_value	value = def.def->vals[desc.dscptr.member];
	struct jy_func *ofunc = value.func;

	uint32_t *child	  = asts->child[ast];
	uint32_t  childsz = asts->childsz[ast];

	assert(childsz > 1 && "Missing identifier");

	// -1 to not include identifier
	if (childsz - 1 != ofunc->param_size) {
		tkn_error(errs, "invalid signature", tkn, tkn);
		goto PANIC;
	}

	// start from 1 cuz child[0] is the identifier
	for (uint32_t i = 1; i < childsz; ++i) {
		uint32_t      chid   = child[i];
		enum jy_ktype expect = ofunc->param_types[i - 1];
		struct expr   pexpr  = { 0 };

		if (_expr(asts, tkns, chid, ctx, errs, scope, &pexpr))
			goto PANIC;

		if (expect != pexpr.type) {
			uint32_t chtkn = asts->tkns[chid];
			tkn_error(errs, "bad argument type mismatch", tkn,
				  chtkn);
			goto PANIC;
		}
	}

	if (emit_byte(JY_OP_CALL, ctx->codes, ctx->codesz) != 0)
		goto PANIC;
	if (emit_byte(ofunc->param_size, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	expr->type = ofunc->return_type;

	return false;
PANIC:
	return true;
}

static bool _not_expr(const struct jy_asts *asts,
		      const struct jy_tkns *tkns,
		      uint32_t		    ast,
		      struct compiler	   *ctx,
		      struct tkn_errs	   *errs,
		      struct jy_defs	   *scope,
		      struct expr	   *expr)
{
	assert(asts->childsz[ast] == 1);

	uint32_t chid = asts->child[ast][0];

	if (_expr(asts, tkns, chid, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[ast];
		uint32_t to   = asts->tkns[chid];
		tkn_error(errs, "invalid operation", from, to);
		goto PANIC;
	}

	if (emit_byte(JY_OP_NOT, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	return false;
PANIC:
	return true;
}

static bool _and_expr(const struct jy_asts *asts,
		      const struct jy_tkns *tkns,
		      uint32_t		    ast,
		      struct compiler	   *ctx,
		      struct tkn_errs	   *errs,
		      struct jy_defs	   *scope,
		      struct expr	   *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left_id  = asts->child[ast][0];
	uint32_t right_id = asts->child[ast][1];

	if (_expr(asts, tkns, left_id, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[left_id];
		uint32_t to   = asts->tkns[ast];
		tkn_error(errs, "invalid predicate", from, to);
		goto PANIC;
	}

	if (emit_byte(JY_OP_JMPF, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	uint32_t patchofs = *ctx->codesz;

	if (emit_byte(0, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	if (emit_byte(0, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	if (_expr(asts, tkns, right_id, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[right_id];
		uint32_t to   = asts->tkns[right_id];
		tkn_error(errs, "invalid predicate", from, to);
		goto PANIC;
	}

	// TODO: Handle long jump scenario
	short jmp = (uint16_t) (*ctx->codesz - patchofs + 1);

	memcpy(*ctx->codes + patchofs, &jmp, sizeof(jmp));

	return false;
PANIC:
	return true;
}

static bool _or_expr(const struct jy_asts *asts,
		     const struct jy_tkns *tkns,
		     uint32_t		   ast,
		     struct compiler	  *ctx,
		     struct tkn_errs	  *errs,
		     struct jy_defs	  *scope,
		     struct expr	  *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left_id  = asts->child[ast][0];
	uint32_t right_id = asts->child[ast][1];

	if (_expr(asts, tkns, left_id, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[left_id];
		uint32_t to   = asts->tkns[left_id];
		tkn_error(errs, "invalid predicate", from, to);
		goto PANIC;
	}

	if (emit_byte(JY_OP_JMPT, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	uint32_t patchofs = *ctx->codesz;

	if (emit_byte(0, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	if (emit_byte(0, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	if (_expr(asts, tkns, right_id, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[right_id];
		uint32_t to   = asts->tkns[right_id];
		tkn_error(errs, "invalid predicate", from, to);
		goto PANIC;
	}

	// TODO: Handle long jump scenario
	short jmp = (short) (*ctx->codesz - patchofs + 1);
	memcpy(*ctx->codes + patchofs, &jmp, sizeof(jmp));

	return false;
PANIC:
	return true;
}

static bool _exact_expr(const struct jy_asts *asts,
			const struct jy_tkns *tkns,
			uint32_t	      ast,
			struct compiler	     *ctx,
			struct tkn_errs	     *errs,
			struct jy_defs	     *scope,
			struct expr	     *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (asts->types[left] != AST_QACCESS)
		goto INV_LEFT;

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	switch (leftx.type) {
	case JY_K_STR:
	case JY_K_LONG:
		break;
	default:
		goto PANIC;
	}

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (rightx.type != leftx.type)
		goto INV_RIGHT;

	expr->id   = -1u;
	expr->type = JY_K_MATCH;

	if (emit_byte(JY_OP_EQUAL, ctx->codes, ctx->codesz) != 0)
		goto PANIC;

	return false;

INV_LEFT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[left];
	tkn_error(errs, "invalid expression", from, to);
	goto PANIC;
}
INV_RIGHT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	tkn_error(errs, "invalid expression", from, to);
}
PANIC:
	return true;
}

static bool _join_expr(const struct jy_asts *asts,
		       const struct jy_tkns *tkns,
		       uint32_t		     ast,
		       struct compiler	    *ctx,
		       struct tkn_errs	    *errs,
		       struct jy_defs	    *scope,
		       struct expr	    *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (asts->types[left] != AST_QACCESS)
		goto INV_LEFT;

	if (asts->types[right] != AST_QACCESS)
		goto INV_RIGHT;

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (leftx.type != rightx.type)
		goto INV_RIGHT;

	expr->id   = -1u;
	expr->type = JY_K_MATCH;

	if (emit_byte(JY_OP_JOIN, ctx->codes, ctx->codesz))
		goto PANIC;

	return false;

INV_LEFT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[left];
	tkn_error(errs, "invalid expression", from, to);
	goto PANIC;
}
INV_RIGHT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	tkn_error(errs, "invalid expression", from, to);
}
PANIC:
	return true;
}

static bool _equality_expr(const struct jy_asts *asts,
			   const struct jy_tkns *tkns,
			   uint32_t		 ast,
			   struct compiler	*ctx,
			   struct tkn_errs	*errs,
			   struct jy_defs	*scope,
			   struct expr		*expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	enum jy_opcode code;

	switch (leftx.type) {
	case JY_K_LONG:
	case JY_K_BOOL:
		code = JY_OP_CMP;
		break;
	case JY_K_STR:
		code = JY_OP_CMPSTR;
		break;
	default:
		goto INV_EXP;
	}

	if (leftx.type != rightx.type)
		goto INV_EXP;

	expr->id   = -1u;
	expr->type = JY_K_BOOL;

	if (emit_byte(code, ctx->codes, ctx->codesz))
		goto PANIC;

	return false;

INV_EXP: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	tkn_error(errs, "invalid expression", from, to);
}
PANIC:
	return true;
}

static bool _concat_expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       ast,
			 struct compiler      *ctx,
			 struct tkn_errs      *errs,
			 struct jy_defs	      *scope,
			 struct expr	      *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (leftx.type != JY_K_STR)
		goto INV_EXP;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (leftx.type != rightx.type)
		goto INV_EXP;

	if (emit_byte(JY_OP_CONCAT, ctx->codes, ctx->codesz))
		goto PANIC;

	*expr = rightx;

	return false;

INV_EXP: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	tkn_error(errs, "invalid expression", from, to);
}
PANIC:
	return true;
}

static bool _range_expr(const struct jy_asts *asts,
			const struct jy_tkns *tkns,
			uint32_t	      ast,
			struct compiler	     *ctx,
			struct tkn_errs	     *errs,
			struct jy_defs	     *scope,
			struct expr	     *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (leftx.type != JY_K_LONG)
		goto INV_EXP;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (leftx.type != rightx.type)
		goto INV_EXP;

	*expr = rightx;

	return false;

INV_EXP: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	tkn_error(errs, "invalid expression", from, to);
}
PANIC:
	return true;
}

static bool _compare_expr(const struct jy_asts *asts,
			  const struct jy_tkns *tkns,
			  uint32_t		ast,
			  struct compiler      *ctx,
			  struct tkn_errs      *errs,
			  struct jy_defs       *scope,
			  struct expr	       *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (leftx.type != JY_K_LONG)
		goto INV_EXP;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (leftx.type != rightx.type)
		goto INV_EXP;

	enum jy_ast optype = asts->types[ast];
	uint8_t	    code;

	switch (optype) {
	case AST_LESSER:
		code = JY_OP_LT;
		break;
	case AST_GREATER:
		code = JY_OP_GT;
		break;
	default:
		goto INV_EXP;
	}

	if (emit_byte(code, ctx->codes, ctx->codesz))
		goto PANIC;

	expr->id   = -1u;
	expr->type = JY_K_BOOL;

	return false;

INV_EXP: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	tkn_error(errs, "invalid expression", from, to);
}
PANIC:
	return true;
}

static bool _within_expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       ast,
			 struct compiler      *ctx,
			 struct tkn_errs      *errs,
			 struct jy_defs	      *scope,
			 struct expr	      *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (leftx.type != JY_K_EVENT)
		goto INV_LEFT;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (rightx.type != JY_K_TIME)
		goto INV_WITHIN;

	if (emit_byte(JY_OP_WITHIN, ctx->codes, ctx->codesz))
		goto PANIC;

	expr->id   = -1u;
	expr->type = JY_K_MATCH;

	return false;

INV_LEFT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[left];
	tkn_error(errs, "invalid expression", from, to);
	goto PANIC;
}

INV_WITHIN: {
	uint32_t from = asts->tkns[ast];
	uint32_t to   = asts->tkns[ast];
	tkn_error(errs, "invalid expression", from, to);
}
PANIC:
	return true;
}

static bool _regex_expr(const struct jy_asts *asts,
			const struct jy_tkns *tkns,
			uint32_t	      ast,
			struct compiler	     *ctx,
			struct tkn_errs	     *errs,
			struct jy_defs	     *scope,
			struct expr	     *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (leftx.type != JY_K_STR)
		goto INV_LEFT;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (rightx.type != JY_K_REGEX)
		goto INV_RIGHT;

	if (emit_byte(JY_OP_REGEX, ctx->codes, ctx->codesz))
		goto PANIC;

	expr->id   = -1u;
	expr->type = JY_K_MATCH;

	return false;

INV_LEFT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[left];
	tkn_error(errs, "expected a string lhs", from, to);
	goto PANIC;
}
INV_RIGHT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	tkn_error(errs, "invalid regex expression", from, to);
	goto PANIC;
}
PANIC:
	return true;
}

static bool _between_expr(const struct jy_asts *asts,
			  const struct jy_tkns *tkns,
			  uint32_t		ast,
			  struct compiler      *ctx,
			  struct tkn_errs      *errs,
			  struct jy_defs       *scope,
			  struct expr	       *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (asts->types[right] != AST_CONCAT)
		goto INV_RIGHT;

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (leftx.type != JY_K_LONG)
		goto INV_LEFT;

	if (_range_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (rightx.type != JY_K_LONG)
		goto INV_BETWEEN;

	if (emit_byte(JY_OP_BETWEEN, ctx->codes, ctx->codesz))
		goto PANIC;

	expr->id   = -1u;
	expr->type = JY_K_MATCH;

	return false;

INV_LEFT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[left];
	tkn_error(errs, "invalid expression", from, to);
	goto PANIC;
}
INV_RIGHT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	tkn_error(errs, "invalid expression", from, to);
	goto PANIC;
}
INV_BETWEEN: {
	uint32_t from = asts->tkns[ast];
	uint32_t to   = asts->tkns[ast];
	tkn_error(errs, "invalid expression", from, to);
}
PANIC:
	return true;
}

static bool _arith_expr(const struct jy_asts *asts,
			const struct jy_tkns *tkns,
			uint32_t	      ast,
			struct compiler	     *ctx,
			struct tkn_errs	     *errs,
			struct jy_defs	     *scope,
			struct expr	     *expr)
{
	assert(asts->childsz[ast] == 2);

	uint32_t left  = asts->child[ast][0];
	uint32_t right = asts->child[ast][1];

	struct expr leftx  = { 0 };
	struct expr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (leftx.type != JY_K_LONG)
		goto INV_EXP;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (leftx.type != rightx.type)
		goto INV_EXP;

	enum jy_ast optype = asts->types[ast];
	uint8_t	    code;

	switch (optype) {
	case AST_ADDITION:
		code = JY_OP_ADD;
		break;
	case AST_SUBTRACT:
		code = JY_OP_SUB;
		break;
	case AST_MULTIPLY:
		code = JY_OP_MUL;
		break;
	case AST_DIVIDE:
		code = JY_OP_DIV;
		break;
	default:
		goto INV_EXP;
	}

	if (emit_byte(code, ctx->codes, ctx->codesz))
		goto PANIC;

	*expr = rightx;

	return false;

INV_EXP: {
	uint32_t from = asts->tkns[ast];
	uint32_t to   = asts->tkns[ast];
	tkn_error(errs, "invalid expression", from, to);
}
PANIC:
	return true;
}

static cmplfn_t rules[TOTAL_AST_TYPES] = {
	[AST_CALL] = _call_expr,

	[AST_NOT] = _not_expr,
	[AST_AND] = _and_expr,
	[AST_OR]  = _or_expr,

	// > binaries
	[AST_JOINX]   = _join_expr,
	[AST_EXACT]   = _exact_expr,
	[AST_EQUAL]   = _exact_expr,
	[AST_BETWEEN] = _between_expr,
	[AST_WITHIN]  = _within_expr,
	[AST_REGEX]   = _regex_expr,

	[AST_EQUALITY] = _equality_expr,
	[AST_LESSER]   = _compare_expr,
	[AST_GREATER]  = _compare_expr,

	[AST_CONCAT]   = _concat_expr,
	[AST_ADDITION] = _arith_expr,
	[AST_SUBTRACT] = _arith_expr,
	[AST_MULTIPLY] = _arith_expr,
	[AST_DIVIDE]   = _arith_expr,
	// < binaries

	[AST_NAME]    = _descriptor_expr,
	[AST_EVENT]   = _descriptor_expr,
	[AST_QACCESS] = _qaccess_expr,
	[AST_EACCESS] = _eaccess_expr,

	// > literal
	[AST_REGEXP] = _regexp_expr,
	[AST_LONG]   = _long_expr,
	[AST_STRING] = _string_expr,
	[AST_HOUR]   = _time_expr,
	[AST_MINUTE] = _time_expr,
	[AST_SECOND] = _time_expr,
	[AST_FALSE]  = _bool_expr,
	[AST_TRUE]   = _bool_expr,
	// < literal
};

static inline cmplfn_t rule_expression(enum jy_ast type)
{
	assert(rules[type] != NULL);

	return rules[type];
}

static inline bool _match_sect(const struct jy_asts *asts,
			       const struct jy_tkns *tkns,
			       uint32_t		     sect,
			       struct compiler	    *ctx,
			       struct tkn_errs	    *errs)
{
	uint32_t  sectkn  = asts->tkns[sect];
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t    chid  = child[i];
		uint32_t    chtkn = asts->tkns[chid];
		struct expr expr  = { 0 };

		if (_expr(asts, tkns, chid, ctx, errs, ctx->names, &expr))
			continue;

		if (expr.type != JY_K_MATCH) {
			tkn_error(errs, "invalid match expression", sectkn,
				  chtkn);
			continue;
		}
	}

	return false;
}

static inline bool _condition_sect(const struct jy_asts *asts,
				   const struct jy_tkns *tkns,
				   uint32_t		 sect,
				   struct compiler	*ctx,
				   struct tkn_errs	*errs,
				   uint32_t	       **patchofs,
				   uint32_t		*patchsz)
{
	uint32_t  sectkn  = asts->tkns[sect];
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	if (childsz > 255) {
		tkn_error(errs, "too many conditions", sectkn, sectkn);
		goto PANIC;
	}

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t    chid  = child[i];
		uint32_t    chtkn = asts->tkns[chid];
		struct expr expr  = { 0 };

		if (_expr(asts, tkns, chid, ctx, errs, ctx->names, &expr))
			continue;

		if (expr.type != JY_K_BOOL) {
			tkn_error(errs, "invalid condition expression", sectkn,
				  chtkn);
			continue;
		}

		if (emit_byte(JY_OP_JMPF, ctx->codes, ctx->codesz) != 0)
			goto PANIC;

		jry_mem_push(*patchofs, *patchsz, *ctx->codesz);

		if (patchofs == NULL)
			goto PANIC;

		*patchsz += 1;

		// Reserved 2 bytes for short jump
		if (emit_byte(0, ctx->codes, ctx->codesz) != 0)
			goto PANIC;

		if (emit_byte(0, ctx->codes, ctx->codesz) != 0)
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static inline bool _action_sect(const struct jy_asts *asts,
				const struct jy_tkns *tkns,
				uint32_t	      sect,
				struct compiler	     *ctx,
				struct tkn_errs	     *errs)
{
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t    chid = child[i];
		struct expr expr = { 0 };

		_expr(asts, tkns, chid, ctx, errs, ctx->names, &expr);

		if (expr.type != JY_K_ACTION) {
			uint32_t from = asts->tkns[sect];
			uint32_t to   = asts->tkns[chid];
			tkn_error(errs, "invalid target expression", from, to);
			goto PANIC;
		}
	}

	return false;
PANIC:
	return true;
}

static inline bool _output_sect(const struct jy_asts *asts,
				const struct jy_tkns *tkns,
				uint32_t	      sect,
				struct compiler	     *ctx,
				struct tkn_errs	     *errs)
{
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t    chid = child[i];
		struct expr expr = { 0 };

		_expr(asts, tkns, chid, ctx, errs, ctx->names, &expr);

		switch (expr.type) {
		case JY_K_LONG:
		case JY_K_ULONG:
		case JY_K_BOOL:
		case JY_K_STR:
			break;
		default: {
			uint32_t from = asts->tkns[sect];
			uint32_t to   = asts->tkns[chid];
			tkn_error(errs, "invalid output expression", from, to);
			goto PANIC;
		}
		}
	}

	size_t		valsz	 = *ctx->valsz;
	union jy_value *vals	 = *ctx->vals;
	enum jy_ktype  *types	 = *ctx->types;
	uint32_t	outsz_id = valsz;

	for (uint32_t i = 0; i < valsz; ++i) {
		enum jy_ktype t = types[i];
		long	      v = vals[i].i64;

		if (t != JY_K_ULONG || v != childsz)
			continue;

		goto EMIT_OUTPUT;
	}

	union jy_value v = { .i64 = childsz };

	if (emit_cnst(v, JY_K_ULONG, ctx->vals, ctx->types, ctx->valsz))
		goto OUT_OF_MEMORY;

EMIT_OUTPUT:
	if (emit_push(outsz_id, ctx->codes, ctx->codesz))
		goto OUT_OF_MEMORY;

	if (emit_byte(JY_OP_OUTPUT, ctx->codes, ctx->codesz))
		goto OUT_OF_MEMORY;

	return false;
PANIC:
	return true;

OUT_OF_MEMORY:
	return true;
}

static inline bool _field_sect(const struct jy_asts *asts,
			       const struct jy_tkns *tkns,
			       struct tkn_errs	    *errs,
			       uint32_t		     id,
			       struct jy_defs	    *def)
{
	uint32_t *child	  = asts->child[id];
	uint32_t  childsz = asts->childsz[id];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t name_id = child[i];
		uint32_t type_id = asts->child[name_id][0];
		char	*name	 = tkns->lexemes[asts->tkns[name_id]];

		if (def_find(def, name, NULL)) {
			uint32_t from = asts->tkns[id];
			uint32_t to   = asts->tkns[type_id];
			tkn_error(errs, "field redefinition", from, to);
			continue;
		}

		enum jy_ktype  ktype = JY_K_UNKNOWN;
		union jy_value value = { .handle = NULL };
		enum jy_ast    type  = asts->types[type_id];

		switch (type) {
		case AST_LONG_TYPE:
			ktype = JY_K_LONG;
			break;
		case AST_STR_TYPE:
			ktype = JY_K_STR;
			break;
		case AST_BOOL_TYPE:
			ktype = JY_K_BOOL;
			break;
		default: {
			uint32_t from = asts->tkns[id];
			uint32_t to   = asts->tkns[name_id];
			tkn_error(errs, "invalid type", from, to);
			goto PANIC;
		}
		}

		if (def_add(def, name, value, ktype))
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static inline int emit_query(uint16_t	     *valsz,
			     union jy_value **vals,
			     enum jy_ktype  **types,
			     uint32_t	     *codesz,
			     uint8_t	    **codes,
			     long	      qlen,
			     size_t	      chunkid)
{
	uint32_t id = *valsz;

	for (uint32_t i = 0; i < *valsz; ++i) {
		enum jy_ktype t = (*types)[i];
		long	      v = (*vals)[i].i64;

		if (t != JY_K_LONG || v != qlen)
			continue;

		id = i;
		goto EMIT_QUERY;
	}

	union jy_value v = { .i64 = qlen };

	if (emit_cnst(v, JY_K_LONG, vals, types, valsz))
		goto OUT_OF_MEMORY;

EMIT_QUERY:
	if (emit_push(id, codes, codesz))
		goto OUT_OF_MEMORY;

	if (emit_push(chunkid, codes, codesz))
		goto OUT_OF_MEMORY;

	if (emit_byte(JY_OP_QUERY, codes, codesz))
		goto OUT_OF_MEMORY;

	return 0;
OUT_OF_MEMORY:
	return -1;
}

static inline bool _rule_decl(const struct jy_asts *asts,
			      const struct jy_tkns *tkns,
			      struct jy_jay	   *jay,
			      struct tkn_errs	   *errs,
			      uint32_t		    rule)
{
	bool panic = false;

	struct sc_mem scratch = { .buf = NULL };
	uint32_t      ruletkn = asts->tkns[rule];
	const char   *rulelex = tkns->lexemes[ruletkn];
	uint32_t     *child   = asts->child[rule];
	uint32_t      childsz = asts->childsz[rule];

	// Short jump patch offset
	uint32_t *patchofs = NULL;
	uint32_t  patchsz  = 0;

	if (childsz >= 255) {
		tkn_error(errs, "too many sections", ruletkn, ruletkn);
		goto PANIC;
	}

	uint32_t matchs[255];
	uint32_t actions[255];
	uint32_t conds[255];
	uint32_t outputs[255];

	uint32_t	matchsz	 = 0;
	uint32_t	actionsz = 0;
	uint32_t	condsz	 = 0;
	uint32_t	outputsz = 0;
	struct jy_defs *names	 = jay->names;
	unsigned long	rulecofs = jay->codesz;
	uint32_t	rulenid;

	if (def_find(names, rulelex, NULL)) {
		tkn_error(errs, "duplicate definition", ruletkn, ruletkn);
		goto PANIC;
	}

	union jy_value view = { .ofs = jay->rulesz };

	if (def_add(names, rulelex, view, JY_K_RULE))
		goto OUT_OF_MEMORY;

	def_find(names, rulelex, &rulenid);

	unsigned long fstart = jay->fcodesz;

	jry_mem_push(jay->rulecofs, jay->rulesz, rulecofs);

	if (jay->rulecofs == NULL)
		goto OUT_OF_MEMORY;

	jry_mem_push(jay->rulenids, jay->rulesz, rulenid);

	if (jay->rulenids == NULL)
		goto OUT_OF_MEMORY;

	jay->rulesz += 1;

	if (sc_reap(&scratch, &patchofs, (free_t) ifree))
		goto OUT_OF_MEMORY;

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t    chid = child[i];
		enum jy_ast type = asts->types[chid];

		switch (type) {
		case AST_MATCH_SECT:
			matchs[matchsz++] = chid;
			break;
		case AST_JUMP_SECT:
			actions[actionsz++] = chid;
			break;
		case AST_CONDITION_SECT:
			conds[condsz++] = chid;
			break;
		case AST_OUTPUT_SECT:
			outputs[outputsz++] = chid;
			break;
		default: {
			uint32_t to = asts->tkns[chid];
			tkn_error(errs, "invalid rule section", ruletkn, to);
		}
			continue;
		}
	}

	// TODO: implement multiple match section
	if (matchsz > 1) {
		tkn_error(errs, "too many match section", ruletkn, ruletkn);
		goto PANIC;
	}

	// TODO: implement multiple condition section
	if (condsz > 1) {
		tkn_error(errs, "too many condition section", ruletkn, ruletkn);
		goto PANIC;
	}

	// TODO: implement multiple action section
	if (actionsz > 1) {
		tkn_error(errs, "too many action section", ruletkn, ruletkn);
		goto PANIC;
	}

	// TODO: implement multiple output section
	if (outputsz > 1) {
		tkn_error(errs, "too many output section", ruletkn, ruletkn);
		goto PANIC;
	}

	struct compiler ctx = {
		.names = jay->names,
		.vals  = &jay->vals,
		.types = &jay->types,
		.valsz = &jay->valsz,
	};

	ctx.codes  = &jay->fcodes;
	ctx.codesz = &jay->fcodesz;

	for (uint32_t i = 0; i < condsz; ++i) {
		uint32_t id = conds[i];
		_condition_sect(asts, tkns, id, &ctx, errs, &patchofs,
				&patchsz);
	}

	for (uint32_t i = 0; i < outputsz; ++i) {
		uint32_t id = outputs[i];
		_output_sect(asts, tkns, id, &ctx, errs);
	}

	for (uint32_t i = 0; i < actionsz; ++i) {
		uint32_t id = actions[i];
		_action_sect(asts, tkns, id, &ctx, errs);
	}

	// patch jumps to END
	for (uint32_t i = 0; i < patchsz; ++i) {
		uint32_t ofs = patchofs[i];
		// TODO: Handle long jump scenario
		short	 jmp = (short) (*ctx.codesz - ofs + 1);

		memcpy(*ctx.codes + ofs, &jmp, sizeof(jmp));
	}

	if (emit_byte(JY_OP_END, ctx.codes, ctx.codesz))
		goto OUT_OF_MEMORY;

	union jy_value	ofsv   = { .ofs = fstart };
	uint32_t	ofskid = *ctx.valsz;
	size_t		valsz  = *ctx.valsz;
	union jy_value *vals   = *ctx.vals;
	enum jy_ktype  *types  = *ctx.types;

	for (uint32_t i = 0; i < valsz; ++i) {
		if (types[i] != JY_K_OFS)
			continue;

		if (vals[i].ofs == ofsv.ofs) {
			ofskid = i;
			goto EMIT_QUERY;
		}
	}

	if (emit_cnst(ofsv, JY_K_OFS, ctx.vals, ctx.types, ctx.valsz))
		goto OUT_OF_MEMORY;

EMIT_QUERY:
	ctx.codes  = &jay->codes;
	ctx.codesz = &jay->codesz;

	long qlen = 0;

	if (matchsz == 0) {
		tkn_error(errs, "rule is missing match section", ruletkn,
			  ruletkn);

		goto OUT_OF_MEMORY;
	}

	for (uint32_t i = 0; i < matchsz; ++i) {
		uint32_t id  = matchs[i];
		qlen	    += asts->childsz[id];

		_match_sect(asts, tkns, id, &ctx, errs);
	}

	if (emit_query(ctx.valsz, ctx.vals, ctx.types, ctx.codesz, ctx.codes,
		       qlen, ofskid))
		goto OUT_OF_MEMORY;

	if (emit_byte(JY_OP_END, ctx.codes, ctx.codesz) != 0)
		return true;

	goto FINISH;

PANIC:
OUT_OF_MEMORY:
	// TODO: write oom error
	panic = true;
	goto FINISH;

FINISH:
	sc_free(&scratch);
	return panic;
}

static inline bool _ingress_decl(struct sc_mem	      *alloc,
				 const struct jy_asts *asts,
				 const struct jy_tkns *tkns,
				 struct jy_jay	      *jay,
				 struct tkn_errs      *errs,
				 uint32_t	       id)
{
	uint32_t  tkn	  = asts->tkns[id];
	uint32_t *child	  = asts->child[id];
	uint32_t  childsz = asts->childsz[id];

	char  *lex   = tkns->lexemes[asts->tkns[id]];
	size_t lexsz = tkns->lexsz[asts->tkns[id]];

	uint32_t fields[childsz];
	uint32_t fieldsz = 0;

	if (def_find(jay->names, lex, NULL)) {
		tkn_error(errs, "field redefinition", tkn, tkn);
		goto PANIC;
	}

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t    chid = child[i];
		enum jy_ast type = asts->types[chid];

		switch (type) {
		case AST_FIELD_SECT:
			fields[fieldsz++] = chid;
			break;
		default:
			break;
		}
	}

	union jy_value v = { .def = sc_alloc(alloc, sizeof *v.def) };

	if (v.def == NULL)
		goto PANIC;

	if (sc_reap(alloc, v.def, (free_t) def_free))
		goto PANIC;

	union jy_value _name_ = {
		.str = sc_alloc(alloc, sizeof(struct jy_str) + lexsz + 1),
	};

	if (_name_.str == NULL)
		goto PANIC;

	_name_.str->size = lexsz;
	memcpy(_name_.str->cstr, lex, lexsz);
	_name_.str->cstr[lexsz] = '\0';

	if (def_add(v.def, "__name__", _name_, JY_K_STR))
		goto PANIC;

	union jy_value null = { .handle = NULL };

	if (def_add(v.def, "__arrival__", null, JY_K_LONG))
		goto PANIC;

	for (uint32_t i = 0; i < fieldsz; ++i)
		if (_field_sect(asts, tkns, errs, fields[i], v.def))
			goto PANIC;

	if (emit_cnst(v, JY_K_EVENT, &jay->vals, &jay->types, &jay->valsz))
		goto PANIC;

	if (def_add(jay->names, lex, v, JY_K_EVENT))
		goto PANIC;

	return false;

PANIC:
	return true;
}

static inline bool _import_stmt(struct sc_mem	     *alloc,
				const struct jy_asts *asts,
				const struct jy_tkns *tkns,
				const char	     *mdir,
				struct jy_jay	     *jay,
				struct tkn_errs	     *errs,
				uint32_t	      id)
{
	uint32_t tkn	  = asts->tkns[id];
	char	*lexeme	  = tkns->lexemes[tkn];
	uint32_t lexsz	  = tkns->lexsz[tkn];
	int	 dirlen	  = strlen(mdir);
	char	 prefix[] = "lib";

	char path[dirlen + sizeof(prefix) - 1 + lexsz];

	strcpy(path, mdir);
	strcat(path, prefix);
	strncat(path, lexeme, lexsz);

	struct jy_defs def     = { .keys = NULL };
	const char    *msg     = NULL;
	int	       errcode = jry_dlload(path, &def, &msg);

	if (errcode != 0) {
		tkn_error(errs, msg, tkn, tkn);
		goto PANIC;
	}

	union jy_value module = {
		.module = sc_alloc(alloc, sizeof(def)),
	};

	const enum jy_ktype type = JY_K_MODULE;

	if (module.module == NULL)
		goto PANIC;

	*module.module = def;

	if (sc_reap(alloc, module.module, (free_t) def_free))
		goto PANIC;

	if (def_add(jay->names, lexeme, module, type))
		goto PANIC;

	if (emit_cnst(module, type, &jay->vals, &jay->types, &jay->valsz))
		goto PANIC;

	return false;
PANIC:
	return true;
}

static inline bool _root(struct sc_mem	      *alloc,
			 const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 const char	      *mdir,
			 struct jy_jay	      *ctx,
			 struct tkn_errs      *errs,
			 uint32_t	       id)
{
	uint32_t *child	  = asts->child[id];
	uint32_t  childsz = asts->childsz[id];

	uint32_t ingress[childsz];
	uint32_t rules[childsz];
	uint32_t imports[childsz];

	uint32_t imports_sz = 0;
	uint32_t ingress_sz = 0;
	uint32_t rules_sz   = 0;

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t    chid = child[i];
		enum jy_ast decl = asts->types[chid];

		switch (decl) {
		case AST_RULE_DECL:
			rules[rules_sz]	 = chid;
			rules_sz	+= 1;
			break;
		case AST_INGRESS_DECL:
			ingress[ingress_sz]  = chid;
			ingress_sz	    += 1;
			break;
		case AST_IMPORT_STMT:
			imports[imports_sz]  = chid;
			imports_sz	    += 1;
			break;
		default:
			break;
		}
	}

	for (uint32_t i = 0; i < imports_sz; ++i)
		_import_stmt(alloc, asts, tkns, mdir, ctx, errs, imports[i]);

	for (uint32_t i = 0; i < ingress_sz; ++i)
		_ingress_decl(alloc, asts, tkns, ctx, errs, ingress[i]);

	for (uint32_t i = 0; i < rules_sz; ++i)
		_rule_decl(asts, tkns, ctx, errs, rules[i]);

	return false;
}

static void free_jay(struct jy_jay *ctx)
{
	for (uint32_t i = 0; i < ctx->valsz; ++i) {
		union jy_value v = ctx->vals[i];

		switch (ctx->types[i]) {
		case JY_K_STR:
			jry_free(v.str);
			break;
		default:
			continue;
		}
	}

	jry_free(ctx->codes);
	jry_free(ctx->fcodes);
	jry_free(ctx->vals);
	jry_free(ctx->types);
	jry_free(ctx->rulenids);
	jry_free(ctx->rulecofs);

	for (uint32_t i = 0; i < ctx->names->capacity; ++i) {
		union jy_value v    = ctx->names->vals[i];
		enum jy_ktype  type = ctx->names->types[i];

		switch (type) {
		case JY_K_MODULE:
			// TODO: seperate unloading from free
			jry_dlunload(v.module, NULL);
			break;
		default:
			continue;
		}
	}
}

int jry_compile(struct sc_mem	     *alloc,
		struct jy_jay	     *ctx,
		struct tkn_errs	     *errs,
		const char	     *mdir,
		const struct jy_asts *asts,
		const struct jy_tkns *tkns)
{
	assert(ctx->names == NULL);
	assert(mdir != NULL);

	uint32_t    root = 0;
	enum jy_ast type = asts->types[root];

	if (type != AST_ROOT)
		goto INVARIANT;

	ctx->names = sc_alloc(alloc, sizeof *ctx->names);

	if (ctx->names == NULL)
		goto OUT_OF_MEMORY;

	if (sc_reap(alloc, ctx->names, (free_t) def_free))
		goto OUT_OF_MEMORY;

	union jy_value val = { .def = ctx->names };

	if (emit_cnst(val, JY_K_MODULE, &ctx->vals, &ctx->types, &ctx->valsz))
		goto OUT_OF_MEMORY;

	val.i64 = 0;

	if (emit_cnst(val, JY_K_LONG, &ctx->vals, &ctx->types, &ctx->valsz))
		goto OUT_OF_MEMORY;

	val.i64 = 1;

	if (emit_cnst(val, JY_K_LONG, &ctx->vals, &ctx->types, &ctx->valsz))
		goto OUT_OF_MEMORY;

	_root(alloc, asts, tkns, mdir, ctx, errs, root);

	if (sc_reap(alloc, ctx, (free_t) free_jay))
		goto OUT_OF_MEMORY;

OUT_OF_MEMORY:
	return 1;

INVARIANT:
	return 2;
}
