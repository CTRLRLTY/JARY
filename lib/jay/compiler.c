#include "compiler.h"

#include "dload.h"

#include "jary/error.h"
#include "jary/memory.h"
#include "jary/object.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char msg_no_definition[]	  = "undefined";
static const char msg_inv_type[]	  = "invalid type";
static const char msg_inv_signature[]	  = "invalid signature";
static const char msg_inv_rule_sect[]	  = "invalid rule section";
static const char msg_inv_match_expr[]	  = "invalid match expression";
static const char msg_inv_cond_expr[]	  = "invalid condition expression";
static const char msg_inv_target_expr[]	  = "invalid target expression";
static const char msg_inv_operation[]	  = "invalid operation";
static const char msg_inv_expression[]	  = "invalid expression";
static const char msg_inv_predicate[]	  = "invalid predicate";
static const char msg_argument_mismatch[] = "bad argument type mismatch";
static const char msg_type_mismatch[]	  = "type mismatch";
static const char msg_redefinition[]	  = "field redefinition";

struct kexpr {
	// Good luck tracing this, cuz its FUCKED!
	// This id can be any id.
	uint32_t      id;
	enum jy_ktype type;
};

// expression compile subroutine signature
typedef bool (*cmplfn_t)(const struct jy_asts *,
			 const struct jy_tkns *,
			 uint32_t,
			 struct jy_jay *,
			 struct jy_errs *,
			 struct jy_defs *,
			 struct kexpr *);

static inline cmplfn_t rule_expression(enum jy_ast type);

__use_result static inline int emit_byte(uint8_t   code,
					 uint8_t **codes,
					 uint32_t *codesz)
{
	jry_mem_push(*codes, *codesz, code);

	if (*codes == NULL)
		return ERROR_NOMEM;

	*codesz += 1;

	return ERROR_SUCCESS;
}

__use_result static int emit_push(uint32_t  constant,
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

__use_result static int emit_cnst(union jy_value   value,
				  enum jy_ktype	   type,
				  union jy_value **vals,
				  enum jy_ktype	 **types,
				  uint16_t	  *length)
{
	jry_mem_push(*vals, *length, value);
	jry_mem_push(*types, *length, type);

	if (*vals == NULL || *types == NULL)
		return ERROR_NOMEM;

	*length += 1;

	return ERROR_SUCCESS;
}

static inline bool _expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       id,
			 struct jy_jay	      *ctx,
			 struct jy_errs	      *errs,
			 struct jy_defs	      *scope,
			 struct kexpr	      *expr)
{
	enum jy_ast type = asts->types[id];
	cmplfn_t    fn	 = rule_expression(type);
	jry_assert(fn != NULL);

	return fn(asts, tkns, id, ctx, errs, scope, expr);
}

static bool _long_expr(const struct jy_asts *asts,
		       const struct jy_tkns *tkns,
		       uint32_t		     id,
		       struct jy_jay	    *ctx,
		       struct jy_errs	    *__unused(errs),
		       struct jy_defs	    *__unused(scope),
		       struct kexpr	    *expr)
{
	uint32_t tkn	   = asts->tkns[id];
	char	*lexeme	   = tkns->lexemes[tkn];

	union jy_value num = { .i64 = strtol(lexeme, NULL, 10) };
	expr->type	   = JY_K_LONG;

	for (uint32_t i = 0; i < ctx->valsz; ++i) {
		enum jy_ktype t = ctx->types[i];
		long	      v = ctx->vals[i].i64;

		if (t != JY_K_LONG || v != num.i64)
			continue;

		expr->id = i;

		goto DONE;
	}

	expr->id = ctx->valsz;

	if (emit_cnst(num, JY_K_LONG, &ctx->vals, &ctx->types, &ctx->valsz) !=
	    0)
		goto PANIC;

DONE:
	if (emit_push(expr->id, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	return false;

PANIC:
	return true;
}

static bool _string_expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       id,
			 struct jy_jay	      *ctx,
			 struct jy_errs	      *__unused(errs),
			 struct jy_defs	      *__unused(scope),
			 struct kexpr	      *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];
	uint32_t lexsz	= tkns->lexsz[tkn];

	for (uint32_t i = 0; i < ctx->valsz; ++i) {
		if (ctx->types[i] != JY_K_STR)
			continue;

		struct jy_obj_str *v = ctx->vals[i].str;

		if (v->size != lexsz || memcmp(v->cstr, lexeme, lexsz))
			continue;

		expr->id = i;

		goto EMIT;
	}

	uint32_t       allocsz = sizeof(struct jy_obj_str) + lexsz + 1;
	union jy_value value   = { .str = jry_alloc(allocsz) };

	if (value.str == NULL)
		goto PANIC;

	value.str->size = lexsz;
	memcpy(value.str->cstr, lexeme, lexsz);
	value.str->cstr[lexsz] = '\0';
	expr->id	       = ctx->valsz;

	if (emit_cnst(value, JY_K_STR, &ctx->vals, &ctx->types, &ctx->valsz) !=
	    0)
		goto PANIC;
EMIT:
	if (emit_push(expr->id, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	expr->type = JY_K_STR;

	return false;
PANIC:
	return true;
}

static bool _descriptor_expr(const struct jy_asts *asts,
			     const struct jy_tkns *tkns,
			     uint32_t		   id,
			     struct jy_jay	  *ctx,
			     struct jy_errs	  *errs,
			     struct jy_defs	  *scope,
			     struct kexpr	  *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];

	uint32_t nid;

	if (!jry_find_def(scope, lexeme, &nid)) {
		jry_push_error(errs, msg_no_definition, tkn, tkn);
		goto PANIC;
	}

	union jy_value value;
	uint32_t       val_k_id	  = ctx->valsz;
	uint32_t       scope_k_id = -1u;

	for (uint32_t i = 0; i < ctx->valsz; ++i) {
		switch (ctx->types[i]) {
		case JY_K_MODULE:
		case JY_K_EVENT:
			break;
		default:
			continue;
		}

		if (scope == ctx->vals[i].def) {
			scope_k_id = i;
			break;
		}
	}

	jry_assert(scope_k_id != -1u && "scope not in constant table");

	value.dscptr.name   = scope_k_id;
	value.dscptr.member = nid;

	for (uint32_t i = 0; i < ctx->valsz; ++i) {
		if (JY_K_DESCRIPTOR != ctx->types[i])
			continue;

		if (value.i64 == ctx->vals[i].i64) {
			val_k_id = i;
			goto EMIT;
		}
	}

	if (emit_cnst(value, JY_K_DESCRIPTOR, &ctx->vals, &ctx->types,
		      &ctx->valsz))
		goto PANIC;
EMIT:
	if (emit_push(val_k_id, &ctx->codes, &ctx->codesz))
		goto PANIC;

	expr->id   = val_k_id;
	expr->type = scope->types[nid];

	return false;
PANIC:
	return true;
}

static bool _access_expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       id,
			 struct jy_jay	      *ctx,
			 struct jy_errs	      *errs,
			 struct jy_defs	      *scope,
			 struct kexpr	      *expr)
{
	jry_assert(asts->childsz[id] == 2);

	uint32_t left  = asts->child[id][0];
	uint32_t right = asts->child[id][1];

	uint32_t oldsz = ctx->codesz;

	if (_expr(asts, tkns, left, ctx, errs, scope, expr))
		return true;

	// discard previous codes
	ctx->codesz	       = oldsz;
	union jy_value	desc   = ctx->vals[expr->id];
	struct jy_defs *lscope = ctx->vals[desc.dscptr.name].def;
	lscope		       = lscope->vals[desc.dscptr.member].def;

	if (_expr(asts, tkns, right, ctx, errs, lscope, expr))
		return true;

	return false;
}

static bool _call_expr(const struct jy_asts *asts,
		       const struct jy_tkns *tkns,
		       uint32_t		     ast,
		       struct jy_jay	    *ctx,
		       struct jy_errs	    *errs,
		       struct jy_defs	    *scope,
		       struct kexpr	    *expr)
{
	uint32_t name = asts->child[ast][0];
	uint32_t tkn  = asts->tkns[name];

	if (_expr(asts, tkns, name, ctx, errs, scope, expr))
		goto PANIC;

	enum jy_ktype type = expr->type;

	if (type != JY_K_FUNC) {
		jry_push_error(errs, msg_type_mismatch, tkn, tkn);
		goto PANIC;
	}

	union jy_value	    desc  = ctx->vals[expr->id];
	union jy_value	    def	  = ctx->vals[desc.dscptr.name];
	union jy_value	    value = def.def->vals[desc.dscptr.member];
	struct jy_obj_func *ofunc = value.func;

	uint32_t *child		  = asts->child[ast];
	uint32_t  childsz	  = asts->childsz[ast];

	jry_assert(childsz > 1 && "Missing identifier");

	// -1 to not include identifier
	if (childsz - 1 != ofunc->param_size) {
		jry_push_error(errs, msg_inv_signature, tkn, tkn);
		goto PANIC;
	}

	// start from 1 cuz child[0] is the identifier
	for (uint32_t i = 1; i < childsz; ++i) {
		uint32_t      chid   = child[i];
		enum jy_ktype expect = ofunc->param_types[i - 1];
		struct kexpr  pexpr  = { 0 };

		if (_expr(asts, tkns, chid, ctx, errs, scope, &pexpr))
			goto PANIC;

		if (expect != pexpr.type) {
			uint32_t chtkn = asts->tkns[chid];
			jry_push_error(errs, msg_argument_mismatch, tkn, chtkn);
			goto PANIC;
		}
	}

	if (emit_byte(JY_OP_CALL, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;
	if (emit_byte(ofunc->param_size, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	expr->type = ofunc->return_type;

	return false;
PANIC:
	return true;
}

static bool _not_expr(const struct jy_asts *asts,
		      const struct jy_tkns *tkns,
		      uint32_t		    ast,
		      struct jy_jay	   *ctx,
		      struct jy_errs	   *errs,
		      struct jy_defs	   *scope,
		      struct kexpr	   *expr)
{
	jry_assert(asts->childsz[ast] == 1);

	uint32_t chid = asts->child[ast][0];

	if (_expr(asts, tkns, chid, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[ast];
		uint32_t to   = asts->tkns[chid];
		jry_push_error(errs, msg_inv_operation, from, to);
		goto PANIC;
	}

	if (emit_byte(JY_OP_NOT, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	return false;
PANIC:
	return true;
}

static bool _and_expr(const struct jy_asts *asts,
		      const struct jy_tkns *tkns,
		      uint32_t		    ast,
		      struct jy_jay	   *ctx,
		      struct jy_errs	   *errs,
		      struct jy_defs	   *scope,
		      struct kexpr	   *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t left_id  = asts->child[ast][0];
	uint32_t right_id = asts->child[ast][1];

	if (_expr(asts, tkns, left_id, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[left_id];
		uint32_t to   = asts->tkns[left_id];
		jry_push_error(errs, msg_inv_predicate, from, to);
		goto PANIC;
	}

	if (emit_byte(JY_OP_JMPF, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	uint32_t patchofs = ctx->codesz;

	if (emit_byte(0, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	if (emit_byte(0, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	if (_expr(asts, tkns, right_id, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[right_id];
		uint32_t to   = asts->tkns[right_id];
		jry_push_error(errs, msg_inv_predicate, from, to);
		goto PANIC;
	}

	short jmp = (short) (ctx->codesz - patchofs + 1);
	memcpy(ctx->codes + patchofs, &jmp, sizeof(jmp));

	return false;
PANIC:
	return true;
}

static bool _or_expr(const struct jy_asts *asts,
		     const struct jy_tkns *tkns,
		     uint32_t		   ast,
		     struct jy_jay	  *ctx,
		     struct jy_errs	  *errs,
		     struct jy_defs	  *scope,
		     struct kexpr	  *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t left_id  = asts->child[ast][0];
	uint32_t right_id = asts->child[ast][1];

	if (_expr(asts, tkns, left_id, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[left_id];
		uint32_t to   = asts->tkns[left_id];
		jry_push_error(errs, msg_inv_predicate, from, to);
		goto PANIC;
	}

	if (emit_byte(JY_OP_JMPT, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	uint32_t patchofs = ctx->codesz;

	if (emit_byte(0, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	if (emit_byte(0, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	if (_expr(asts, tkns, right_id, ctx, errs, scope, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[right_id];
		uint32_t to   = asts->tkns[right_id];
		jry_push_error(errs, msg_inv_predicate, from, to);
		goto PANIC;
	}

	short jmp = (short) (ctx->codesz - patchofs + 1);
	memcpy(ctx->codes + patchofs, &jmp, sizeof(jmp));

	return false;
PANIC:
	return true;
}

static bool _exact_expr(const struct jy_asts *asts,
			const struct jy_tkns *tkns,
			uint32_t	      ast,
			struct jy_jay	     *ctx,
			struct jy_errs	     *errs,
			struct jy_defs	     *scope,
			struct kexpr	     *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t left	    = asts->child[ast][0];
	uint32_t right	    = asts->child[ast][1];

	struct kexpr leftx  = { 0 };
	struct kexpr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (leftx.type != JY_K_STR_FIELD)
		goto INV_LEFT;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (rightx.type != JY_K_STR)
		goto INV_RIGHT;

	expr->id   = -1u;
	expr->type = JY_K_MATCH;

	if (emit_byte(JY_OP_EXACT, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	return false;

INV_LEFT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[left];
	jry_push_error(errs, msg_inv_expression, from, to);
	goto PANIC;
}
INV_RIGHT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	jry_push_error(errs, msg_inv_expression, from, to);
}
PANIC:
	return true;
}

static bool _join_expr(const struct jy_asts *asts,
		       const struct jy_tkns *tkns,
		       uint32_t		     ast,
		       struct jy_jay	    *ctx,
		       struct jy_errs	    *errs,
		       struct jy_defs	    *scope,
		       struct kexpr	    *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t left	    = asts->child[ast][0];
	uint32_t right	    = asts->child[ast][1];

	struct kexpr leftx  = { 0 };
	struct kexpr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	switch (leftx.type) {
	case JY_K_STR_FIELD:
	case JY_K_LONG_FIELD:
	case JY_K_BOOL_FIELD:
		break;
	default:
		goto INV_LEFT;
	}

	if (leftx.type != rightx.type)
		goto INV_RIGHT;

	*expr = rightx;

	if (emit_byte(JY_OP_JOIN, &ctx->codes, &ctx->codesz))
		goto PANIC;

	return false;

INV_LEFT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[left];
	jry_push_error(errs, msg_inv_expression, from, to);
	goto PANIC;
}
INV_RIGHT: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	jry_push_error(errs, msg_inv_expression, from, to);
}
PANIC:
	return true;
}

static bool _equal_expr(const struct jy_asts *asts,
			const struct jy_tkns *tkns,
			uint32_t	      ast,
			struct jy_jay	     *ctx,
			struct jy_errs	     *errs,
			struct jy_defs	     *scope,
			struct kexpr	     *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t left	    = asts->child[ast][0];
	uint32_t right	    = asts->child[ast][1];

	struct kexpr leftx  = { 0 };
	struct kexpr rightx = { 0 };

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

	if (emit_byte(code, &ctx->codes, &ctx->codesz))
		goto PANIC;

	return false;

INV_EXP: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	jry_push_error(errs, msg_inv_expression, from, to);
}
PANIC:
	return true;
}

static bool _concat_expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       ast,
			 struct jy_jay	      *ctx,
			 struct jy_errs	      *errs,
			 struct jy_defs	      *scope,
			 struct kexpr	      *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t left	    = asts->child[ast][0];
	uint32_t right	    = asts->child[ast][1];

	struct kexpr leftx  = { 0 };
	struct kexpr rightx = { 0 };

	if (_expr(asts, tkns, left, ctx, errs, scope, &leftx))
		goto PANIC;

	if (leftx.type != JY_K_STR)
		goto INV_EXP;

	if (_expr(asts, tkns, right, ctx, errs, scope, &rightx))
		goto PANIC;

	if (leftx.type != rightx.type)
		goto INV_EXP;

	if (emit_byte(JY_OP_CONCAT, &ctx->codes, &ctx->codesz))
		goto PANIC;

	*expr = rightx;

	return false;

INV_EXP: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	jry_push_error(errs, msg_inv_expression, from, to);
}
PANIC:
	return true;
}

static bool _compare_expr(const struct jy_asts *asts,
			  const struct jy_tkns *tkns,
			  uint32_t		ast,
			  struct jy_jay	       *ctx,
			  struct jy_errs       *errs,
			  struct jy_defs       *scope,
			  struct kexpr	       *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t left	    = asts->child[ast][0];
	uint32_t right	    = asts->child[ast][1];

	struct kexpr leftx  = { 0 };
	struct kexpr rightx = { 0 };

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

	if (emit_byte(code, &ctx->codes, &ctx->codesz))
		goto PANIC;

	expr->id   = -1u;
	expr->type = JY_K_BOOL;

	return false;

INV_EXP: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	jry_push_error(errs, msg_inv_expression, from, to);
}
PANIC:
	return true;
}

static bool _arith_expr(const struct jy_asts *asts,
			const struct jy_tkns *tkns,
			uint32_t	      ast,
			struct jy_jay	     *ctx,
			struct jy_errs	     *errs,
			struct jy_defs	     *scope,
			struct kexpr	     *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t left	    = asts->child[ast][0];
	uint32_t right	    = asts->child[ast][1];

	struct kexpr leftx  = { 0 };
	struct kexpr rightx = { 0 };

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

	if (emit_byte(code, &ctx->codes, &ctx->codesz))
		goto PANIC;

	*expr = rightx;

	return false;

INV_EXP: {
	uint32_t from = asts->tkns[left];
	uint32_t to   = asts->tkns[right];
	jry_push_error(errs, msg_inv_expression, from, to);
}
PANIC:
	return true;
}

static cmplfn_t rules[TOTAL_AST_TYPES] = {
	[AST_CALL]     = _call_expr,

	[AST_NOT]      = _not_expr,
	[AST_AND]      = _and_expr,
	[AST_OR]       = _or_expr,

	// > binaries
	[AST_JOINX]    = _join_expr,
	[AST_EXACT]    = _exact_expr,

	[AST_EQUALITY] = _equal_expr,
	[AST_LESSER]   = _compare_expr,
	[AST_GREATER]  = _compare_expr,

	[AST_CONCAT]   = _concat_expr,
	[AST_ADDITION] = _arith_expr,
	[AST_SUBTRACT] = _arith_expr,
	[AST_MULTIPLY] = _arith_expr,
	[AST_DIVIDE]   = _arith_expr,
	// < binaries

	[AST_NAME]     = _descriptor_expr,
	[AST_EVENT]    = _descriptor_expr,
	[AST_ACCESS]   = _access_expr,

	// > literal
	[AST_REGEXP]   = NULL,
	[AST_LONG]     = _long_expr,
	[AST_STRING]   = _string_expr,
	[AST_FALSE]    = NULL,
	[AST_TRUE]     = NULL,
	// < literal
};

static inline cmplfn_t rule_expression(enum jy_ast type)
{
	jry_assert(rules[type] != NULL);

	return rules[type];
}

static inline bool _match_sect(const struct jy_asts *asts,
			       const struct jy_tkns *tkns,
			       uint32_t		     sect,
			       struct jy_jay	    *ctx,
			       struct jy_errs	    *errs)
{
	uint32_t  sectkn  = asts->tkns[sect];
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t     chid  = child[i];
		uint32_t     chtkn = asts->tkns[chid];
		struct kexpr expr  = { 0 };

		if (_expr(asts, tkns, chid, ctx, errs, ctx->names, &expr))
			continue;

		if (expr.type != JY_K_MATCH) {
			jry_push_error(errs, msg_inv_match_expr, sectkn, chtkn);
			continue;
		}
	}

	uint32_t       k_id = ctx->valsz;
	union jy_value qlen = { .i64 = childsz };

	for (uint32_t i = 0; i < ctx->valsz; ++i) {
		enum jy_ktype t = ctx->types[i];
		long	      v = ctx->vals[i].i64;

		if (t != JY_K_LONG || v != qlen.i64)
			continue;

		k_id = i;
		goto EMIT_QUERY;
	}

	if (emit_cnst(qlen, JY_K_LONG, &ctx->vals, &ctx->types, &ctx->valsz))
		goto PANIC;

EMIT_QUERY:
	if (emit_push(k_id, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	if (emit_byte(JY_OP_QUERY, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	return false;
PANIC:
	return true;
}

static inline bool _condition_sect(const struct jy_asts *asts,
				   const struct jy_tkns *tkns,
				   uint32_t		 sect,
				   struct jy_jay	*ctx,
				   struct jy_errs	*errs,
				   uint32_t	       **patchofs,
				   uint32_t		*patchsz)
{
	uint32_t  sectkn  = asts->tkns[sect];
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t     chid  = child[i];
		uint32_t     chtkn = asts->tkns[chid];
		struct kexpr expr  = { 0 };

		if (_expr(asts, tkns, chid, ctx, errs, ctx->names, &expr))
			continue;

		if (expr.type != JY_K_BOOL) {
			jry_push_error(errs, msg_inv_cond_expr, sectkn, chtkn);
			continue;
		}

		if (emit_byte(JY_OP_JMPF, &ctx->codes, &ctx->codesz) != 0)
			goto PANIC;

		jry_mem_push(*patchofs, *patchsz, ctx->codesz);

		if (patchofs == NULL)
			goto PANIC;

		*patchsz += 1;

		// Reserved 2 bytes for short jump
		if (emit_byte(0, &ctx->codes, &ctx->codesz) != 0)
			goto PANIC;

		if (emit_byte(0, &ctx->codes, &ctx->codesz) != 0)
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static inline bool _target_sect(const struct jy_asts *asts,
				const struct jy_tkns *tkns,
				uint32_t	      sect,
				struct jy_jay	     *ctx,
				struct jy_errs	     *errs)
{
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t     chid = child[i];
		struct kexpr expr = { 0 };

		_expr(asts, tkns, chid, ctx, errs, ctx->names, &expr);

		if (expr.type != JY_K_TARGET) {
			uint32_t from = asts->tkns[sect];
			uint32_t to   = asts->tkns[chid];
			jry_push_error(errs, msg_inv_target_expr, from, to);
			goto PANIC;
		}
	}

	return false;
PANIC:
	return true;
}

static inline bool _field_sect(const struct jy_asts *asts,
			       const struct jy_tkns *tkns,
			       struct jy_errs	    *errs,
			       uint32_t		     id,
			       struct jy_defs	    *def)
{
	uint32_t *child	  = asts->child[id];
	uint32_t  childsz = asts->childsz[id];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t op_id	 = child[i];
		uint32_t name_id = asts->child[op_id][0];
		uint32_t type_id = asts->child[op_id][1];
		char	*name	 = tkns->lexemes[asts->tkns[name_id]];

		if (jry_find_def(def, name, NULL)) {
			uint32_t from = asts->tkns[id];
			uint32_t to   = asts->tkns[op_id];
			jry_push_error(errs, msg_redefinition, from, to);
			continue;
		}

		enum jy_ast   type = asts->types[type_id];
		enum jy_ktype ktype;

		switch (type) {
		case AST_LONG_TYPE:
			ktype = JY_K_LONG_FIELD;
			break;
		case AST_STR_TYPE:
			ktype = JY_K_STR_FIELD;
			break;
		case AST_BOOL_TYPE:
			ktype = JY_K_BOOL_FIELD;
			break;
		default: {
			uint32_t from = asts->tkns[id];
			uint32_t to   = asts->tkns[op_id];
			jry_push_error(errs, msg_inv_type, from, to);
			goto PANIC;
		}
		}

		union jy_value null = { .obj = NULL };

		if (jry_add_def(def, name, null, ktype) != ERROR_SUCCESS)
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static inline bool _rule_decl(const struct jy_asts *asts,
			      const struct jy_tkns *tkns,
			      struct jy_jay	   *ctx,
			      struct jy_errs	   *errs,
			      uint32_t		    rule)
{
	uint32_t  ruletkn  = asts->tkns[rule];
	uint32_t *child	   = asts->child[rule];
	uint32_t  childsz  = asts->childsz[rule];

	// Short jump patch offset
	// this will jump to the end of the current rule
	uint32_t *patchofs = NULL;
	uint32_t  patchsz  = 0;

	uint32_t matchs[childsz];
	uint32_t targets[childsz];
	uint32_t conds[childsz];

	uint32_t matchsz  = 0;
	uint32_t targetsz = 0;
	uint32_t condsz	  = 0;

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t    chid = child[i];
		enum jy_ast type = asts->types[chid];

		switch (type) {
		case AST_MATCH_SECT:
			matchs[matchsz++] = chid;
			break;
		case AST_JUMP_SECT:
			targets[targetsz++] = chid;
			break;
		case AST_CONDITION_SECT:
			conds[condsz++] = chid;
			break;
		default: {
			uint32_t to = asts->tkns[chid];
			jry_push_error(errs, msg_inv_rule_sect, ruletkn, to);
		}
			continue;
		}
	}

	for (uint32_t i = 0; i < matchsz; ++i) {
		uint32_t id = matchs[i];
		_match_sect(asts, tkns, id, ctx, errs);
	}

	for (uint32_t i = 0; i < condsz; ++i) {
		uint32_t id = conds[i];
		_condition_sect(asts, tkns, id, ctx, errs, &patchofs, &patchsz);
	}

	for (uint32_t i = 0; i < targetsz; ++i) {
		uint32_t id = targets[i];
		_target_sect(asts, tkns, id, ctx, errs);
	}

	for (uint32_t i = 0; i < patchsz; ++i) {
		uint32_t ofs = patchofs[i];
		short	 jmp = (short) (ctx->codesz - ofs + 1);

		memcpy(ctx->codes + ofs, &jmp, sizeof(jmp));
	}

	jry_free(patchofs);
	return false;
}

static inline bool _ingress_decl(const struct jy_asts *asts,
				 const struct jy_tkns *tkns,
				 struct jy_jay	      *ctx,
				 struct jy_errs	      *errs,
				 uint32_t	       id)
{
	uint32_t  tkn	  = asts->tkns[id];
	uint32_t *child	  = asts->child[id];
	uint32_t  childsz = asts->childsz[id];

	char  *lex	  = tkns->lexemes[asts->tkns[id]];
	size_t lexsz	  = tkns->lexsz[asts->tkns[id]];

	uint32_t fields[childsz];
	uint32_t fieldsz = 0;

	if (jry_find_def(ctx->names, lex, NULL)) {
		jry_push_error(errs, msg_redefinition, tkn, tkn);
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

	union jy_value v = { .def = calloc(1, sizeof *v.def) };

	if (v.def == NULL)
		goto PANIC;

	uint32_t       allocsz = sizeof(struct jy_obj_str) + lexsz + 1;
	union jy_value _name_  = { .str = jry_alloc(allocsz) };

	if (_name_.str == NULL)
		goto PANIC;

	_name_.str->size = lexsz;
	memcpy(_name_.str->cstr, lex, lexsz);
	_name_.str->cstr[lexsz] = '\0';

	if (jry_add_def(v.def, "__name__", _name_, JY_K_STR))
		goto PANIC;

	for (uint32_t i = 0; i < fieldsz; ++i)
		if (_field_sect(asts, tkns, errs, fields[i], v.def))
			goto PANIC;

	if (emit_cnst(v, JY_K_EVENT, &ctx->vals, &ctx->types, &ctx->valsz))
		goto PANIC;

	if (jry_add_def(ctx->names, lex, v, JY_K_EVENT))
		goto PANIC;

	return false;

PANIC:
	return true;
}

static inline bool _import_stmt(const struct jy_asts *asts,
				const struct jy_tkns *tkns,
				struct jy_jay	     *ctx,
				struct jy_errs	     *errs,
				uint32_t	      id)
{
	uint32_t tkn	  = asts->tkns[id];
	char	*lexeme	  = tkns->lexemes[tkn];
	uint32_t lexsz	  = tkns->lexsz[tkn];
	int	 dirlen	  = strlen(ctx->mdir);
	char	 prefix[] = "lib";

	char path[dirlen + sizeof(prefix) - 1 + lexsz];

	strcpy(path, ctx->mdir);
	strcat(path, prefix);
	strncat(path, lexeme, lexsz);

	struct jy_defs def     = { .keys = NULL };
	int	       errcode = jry_module_load(path, &def);

	if (errcode != 0) {
		const char *msg = jry_module_error(errcode);
		jry_push_error(errs, msg, tkn, tkn);
		goto PANIC;
	}

	union jy_value	    module = { .module = jry_alloc(sizeof(def)) };
	const enum jy_ktype type   = JY_K_MODULE;
	*module.module		   = def;

	if (jry_add_def(ctx->names, lexeme, module, type))
		goto PANIC;

	if (emit_cnst(module, type, &ctx->vals, &ctx->types, &ctx->valsz))
		goto PANIC;

	return false;
PANIC:
	return true;
}

static inline bool _root(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 struct jy_jay	      *ctx,
			 struct jy_errs	      *errs,
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
		_import_stmt(asts, tkns, ctx, errs, imports[i]);

	for (uint32_t i = 0; i < ingress_sz; ++i)
		_ingress_decl(asts, tkns, ctx, errs, ingress[i]);

	for (uint32_t i = 0; i < rules_sz; ++i)
		_rule_decl(asts, tkns, ctx, errs, rules[i]);

	if (emit_byte(JY_OP_END, &ctx->codes, &ctx->codesz) != 0)
		return true;

	return false;
}

void jry_compile(const struct jy_asts *asts,
		 const struct jy_tkns *tkns,
		 struct jy_jay	      *ctx,
		 struct jy_errs	      *errs)
{
	jry_assert(ctx->names == NULL);
	jry_assert(ctx->mdir != NULL);

	uint32_t    root = 0;
	enum jy_ast type = asts->types[root];

	if (type != AST_ROOT)
		return;

	ctx->names = calloc(1, sizeof *ctx->names);

	if (ctx->names == NULL)
		return;

	union jy_value val = { .def = ctx->names };

	if (emit_cnst(val, JY_K_MODULE, &ctx->vals, &ctx->types, &ctx->valsz))
		return;

	_root(asts, tkns, ctx, errs, root);
}

void jry_free_jay(struct jy_jay ctx)
{
	for (uint32_t i = 0; i < ctx.valsz; ++i) {
		union jy_value v = ctx.vals[i];

		switch (ctx.types[i]) {
		case JY_K_STR:
			jry_free(v.str);
			break;
		default:
			continue;
		}
	}

	jry_free(ctx.codes);
	jry_free(ctx.vals);
	jry_free(ctx.types);

	if (ctx.names == NULL)
		return;

	for (uint32_t i = 0; i < ctx.names->capacity; ++i) {
		union jy_value v    = ctx.names->vals[i];
		enum jy_ktype  type = ctx.names->types[i];

		switch (type) {
		case JY_K_MODULE:
			jry_module_unload(v.module);
			jry_free(v.def);
			break;
		case JY_K_EVENT: {
			uint32_t id;
			jry_assert(jry_find_def(v.def, "__name__", NULL));

			jry_find_def(v.def, "__name__", &id);

			jry_free(v.def->vals[id].str);

			jry_free_def(*v.def);
			jry_free(v.def);
			break;
		}
		default:
			continue;
		}
	}

	jry_free_def(*ctx.names);
	jry_free(ctx.names);
}
