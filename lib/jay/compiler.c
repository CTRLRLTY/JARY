#include "compiler.h"

#include "dload.h"

#include "jary/error.h"
#include "jary/memory.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char msg_no_definition[]	    = "undefined";
static const char msg_missing_match_sect[]  = "missing match section";
static const char msg_missing_target_sect[] = "missing target section";
static const char msg_inv_type[]	    = "invalid type";
static const char msg_inv_signature[]	    = "invalid signature";
static const char msg_inv_rule_sect[]	    = "invalid rule section";
static const char msg_inv_match_expr[]	    = "invalid match expression";
static const char msg_inv_cond_expr[]	    = "invalid condition expression";
static const char msg_inv_target_expr[]	    = "invalid target expression";
static const char msg_inv_operation[]	    = "invalid operation";
static const char msg_inv_expression[]	    = "invalid expression";
static const char msg_inv_predicate[]	    = "invalid predicate";
static const char msg_operation_mismatch[]  = "invalid operation type mismatch";
static const char msg_argument_mismatch[]   = "bad argument type mismatch";
static const char msg_type_mismatch[]	    = "type mismatch";
static const char msg_redefinition[]	    = "redefinition of";

struct kexpr {
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

static inline struct jy_defs *defobj(struct jy_object_allocator *alloc)
{
	uint32_t nmemb = sizeof(struct jy_defs);
	void	*ptr   = jry_allocobj(nmemb, nmemb, alloc);

	return memset(ptr, 0, sizeof(struct jy_defs));
}

static struct jy_obj_str *stringobj(const char		       *str,
				    uint32_t			len,
				    struct jy_object_allocator *alloc)
{
	// + 1 to include '\0'
	uint32_t allocsz	= sizeof(struct jy_obj_str) + len + 1;

	struct jy_obj_str *ostr = jry_allocobj(allocsz, allocsz, alloc);

	if (ostr) {
		ostr->str  = (void *) (ostr + 1);
		ostr->size = len;
		memcpy(ostr->str, str, len);
		ostr->str[len] = '\0';
	}

	return ostr;
}

static bool find_ast_type(const struct jy_asts *asts,
			  enum jy_ast		type,
			  uint32_t		from)
{
	enum jy_ast current_type = asts->types[from];

	if (current_type == type)
		return true;

	uint32_t *child	  = asts->child[from];
	uint32_t  childsz = asts->childsz[from];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t chid = child[i];

		if (find_ast_type(asts, type, chid))
			return true;
	}

	return false;
}

__use_result static inline int write_byte(uint8_t   code,
					  uint8_t **codes,
					  uint32_t *codesz)
{
	jry_mem_push(*codes, *codesz, code);

	if (*codes == NULL)
		return ERROR_NOMEM;

	*codesz += 1;

	return ERROR_SUCCESS;
}

__use_result static int write_push(uint32_t  constant,
				   uint8_t **code,
				   uint32_t *codesz)
{
	int res = 0;

	if (constant <= 0xff) {
		res = write_byte(JY_OP_PUSH8, code, codesz);
		res = write_byte(constant, code, codesz);

	} else if (constant <= 0xffff) {
		res = write_byte(JY_OP_PUSH16, code, codesz);
		res = write_byte(constant & 0x00FF, code, codesz);
		res = write_byte(constant & 0xFF00, code, codesz);
	}

	return res;
}

__use_result static int write_constant(jy_val_t	       constant,
				       enum jy_ktype   type,
				       jy_val_t	     **vals,
				       enum jy_ktype **types,
				       uint16_t	      *length)
{
	jry_mem_push(*vals, *length, constant);
	jry_mem_push(*types, *length, type);

	if (*vals == NULL || *types == NULL)
		return ERROR_NOMEM;

	*length += 1;

	return ERROR_SUCCESS;
}

static inline bool _expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 struct jy_jay	      *ctx,
			 struct jy_errs	      *errs,
			 struct jy_defs	      *scope,
			 uint32_t	       id,
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
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];

	long num	= strtol(lexeme, NULL, 10);
	expr->type	= JY_K_LONG;

	for (uint32_t i = 0; i < ctx->valsz; ++i) {
		enum jy_ktype t = ctx->types[i];
		long	      v = jry_v2long(ctx->vals[i]);

		if (t != JY_K_LONG || v != num)
			continue;

		expr->id = i;

		goto DONE;
	}

	jy_val_t val = jry_long2v(num);

	expr->id     = ctx->valsz;

	if (write_constant(val, JY_K_LONG, &ctx->vals, &ctx->types,
			   &ctx->valsz) != 0)
		goto PANIC;

DONE:
	if (write_push(expr->id, &ctx->codes, &ctx->codesz) != 0)
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

	expr->type	= JY_K_STR;

	for (uint32_t i = 0; i < ctx->valsz; ++i) {
		enum jy_ktype	   t = ctx->types[i];
		struct jy_obj_str *v = jry_v2str(ctx->vals[i]);

		if (t != JY_K_STR || v->size != lexsz ||
		    v->str[1] != lexeme[1] ||
		    memcmp(v->str + 1, lexeme + 1, v->size - 1) != 0)
			continue;

		expr->id = i;

		goto DONE;
	}

	void *ostr = stringobj(lexeme, lexsz, &ctx->obj);

	if (ostr == NULL)
		goto PANIC;

	jy_val_t v = jry_long2v(memory_offset(ctx->obj.buf, ostr));

	expr->id   = ctx->valsz;

	if (write_constant(v, JY_K_STR, &ctx->vals, &ctx->types, &ctx->valsz) !=
	    0)
		goto PANIC;
DONE:
	if (write_push(expr->id, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	return false;
PANIC:
	return true;
}

static bool _field_expr(const struct jy_asts *asts,
			const struct jy_tkns *tkns,
			uint32_t	      id,
			struct jy_errs	     *errs,
			struct jy_defs	     *scope,
			struct kexpr	     *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];

	uint32_t nid;

	if (!jry_find_def(scope, lexeme, &nid)) {
		jry_push_error(errs, msg_no_definition, tkn, tkn);
		goto PANIC;
	}

	expr->id   = nid;
	expr->type = scope->types[nid];

	return false;
PANIC:
	return true;
}

static bool _name_expr(const struct jy_asts *asts,
		       const struct jy_tkns *tkns,
		       uint32_t		     id,
		       struct jy_jay	    *ctx,
		       struct jy_errs	    *errs,
		       struct jy_defs	    *scope,
		       struct kexpr	    *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];

	uint32_t nid;

	if (!jry_find_def(scope, lexeme, &nid)) {
		jry_push_error(errs, msg_no_definition, tkn, tkn);
		goto PANIC;
	}

	jy_val_t	value = scope->vals[nid];
	struct jy_defs *def   = NULL;

	long ofs	      = jry_v2long(value);
	def		      = memory_fetch(ctx->obj.buf, ofs);
	uint32_t *child	      = asts->child[id];
	uint32_t  childsz     = asts->childsz[id];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t chid = child[i];

		if (_expr(asts, tkns, ctx, errs, def, chid, expr))
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static bool _event_expr(const struct jy_asts *asts,
			const struct jy_tkns *tkns,
			uint32_t	      id,
			struct jy_jay	     *ctx,
			struct jy_errs	     *errs,
			struct jy_defs	     *__unused(scope),
			struct kexpr	     *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];

	uint32_t nid;

	if (!jry_find_def(ctx->names, lexeme, &nid)) {
		jry_push_error(errs, msg_no_definition, tkn, tkn);
		goto PANIC;
	}

	long		ofs   = jry_v2long(ctx->names->vals[nid]);
	struct jy_defs *def   = memory_fetch(ctx->obj.buf, ofs);
	uint32_t       *child = asts->child[id];

	jry_assert(asts->childsz[id] == 1);

	uint32_t chid = child[0];

	if (_field_expr(asts, tkns, chid, errs, def, expr))
		goto PANIC;

	uint32_t event_id = -1u;

	for (uint16_t i = 0; i < ctx->valsz; ++i) {
		if (ctx->types[i] != JY_K_EVENT)
			continue;

		long		ofs  = jry_v2long(ctx->vals[i]);
		struct jy_defs *temp = memory_fetch(ctx->obj.buf, ofs);

		if (def == temp) {
			event_id = i;
			break;
		}
	}

	jry_assert(event_id != -1u);

	if (write_byte(JY_OP_EVENT, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;
	if (write_byte(expr->id & 0x00FF, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;
	if (write_byte(expr->id & 0xFF00, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;
	if (write_byte(event_id & 0x00FF, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;
	if (write_byte(event_id & 0xFF00, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	return false;
PANIC:
	return true;
}

static bool _call_expr(const struct jy_asts *asts,
		       const struct jy_tkns *tkns,
		       uint32_t		     ast,
		       struct jy_jay	    *ctx,
		       struct jy_errs	    *errs,
		       struct jy_defs	    *scope,
		       struct kexpr	    *expr)
{
	uint32_t tkn	= asts->tkns[ast];
	char	*lexeme = tkns->lexemes[tkn];

	uint32_t nid;

	if (!jry_find_def(scope, lexeme, &nid)) {
		jry_push_error(errs, msg_no_definition, tkn, tkn);
		goto PANIC;
	}

	enum jy_ktype type = scope->types[nid];

	if (type != JY_K_FUNC) {
		jry_push_error(errs, msg_type_mismatch, tkn, tkn);
		goto PANIC;
	}

	jy_val_t	    value = scope->vals[nid];
	long		    ofs	  = jry_v2long(value);
	struct jy_obj_func *ofunc = memory_fetch(ctx->obj.buf, ofs);

	uint32_t *child		  = asts->child[ast];
	uint32_t  childsz	  = asts->childsz[ast];

	if (childsz != ofunc->param_size) {
		jry_push_error(errs, msg_inv_signature, tkn, tkn);
		goto PANIC;
	}

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t      chid   = child[i];
		enum jy_ktype expect = ofunc->param_types[i];
		struct kexpr  pexpr  = { 0 };

		if (_expr(asts, tkns, ctx, errs, scope, chid, &pexpr))
			goto PANIC;

		if (expect != pexpr.type) {
			uint32_t chtkn = asts->tkns[chid];
			jry_push_error(errs, msg_argument_mismatch, tkn, chtkn);
			goto PANIC;
		}
	}

	uint32_t call_id = -1u;

	for (uint16_t i = 0; i < ctx->valsz; ++i) {
		if (ctx->types[i] != JY_K_FUNC)
			continue;

		struct jy_obj_func *f = jry_v2func(ctx->vals[i]);

		if (f == ofunc) {
			call_id = i;
			break;
		}
	}

	if (call_id == -1u) {
		call_id = ctx->valsz;
		if (write_constant(value, JY_K_FUNC, &ctx->vals, &ctx->types,
				   &ctx->valsz) != 0)
			goto PANIC;
	}

	if (write_byte(JY_OP_CALL, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;
	if (write_byte(call_id & 0x00FF, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;
	if (write_byte(call_id & 0xFF00, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;
	if (write_byte(ofunc->param_size, &ctx->codes, &ctx->codesz) != 0)
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

	if (_expr(asts, tkns, ctx, errs, scope, chid, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[ast];
		uint32_t to   = asts->tkns[chid];
		jry_push_error(errs, msg_inv_operation, from, to);
		goto PANIC;
	}

	if (write_byte(JY_OP_NOT, &ctx->codes, &ctx->codesz) != 0)
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

	if (_expr(asts, tkns, ctx, errs, scope, left_id, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[left_id];
		uint32_t to   = asts->tkns[left_id];
		jry_push_error(errs, msg_inv_predicate, from, to);
		goto PANIC;
	}

	if (write_byte(JY_OP_JMPF, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	uint32_t patchofs = ctx->codesz;

	if (write_byte(0, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	if (write_byte(0, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	if (_expr(asts, tkns, ctx, errs, scope, right_id, expr))
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

	if (_expr(asts, tkns, ctx, errs, scope, left_id, expr))
		goto PANIC;

	if (expr->type != JY_K_BOOL) {
		uint32_t from = asts->tkns[left_id];
		uint32_t to   = asts->tkns[left_id];
		jry_push_error(errs, msg_inv_predicate, from, to);
		goto PANIC;
	}

	if (write_byte(JY_OP_JMPT, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	uint32_t patchofs = ctx->codesz;

	if (write_byte(0, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	if (write_byte(0, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	if (_expr(asts, tkns, ctx, errs, scope, right_id, expr))
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

static bool _binary_expr(const struct jy_asts *asts,
			 const struct jy_tkns *tkns,
			 uint32_t	       ast,
			 struct jy_jay	      *ctx,
			 struct jy_errs	      *errs,
			 struct jy_defs	      *scope,
			 struct kexpr	      *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t    *child	= asts->child[ast];
	struct kexpr operand[2] = { 0 };

	for (uint32_t i = 0; i < 2; ++i) {
		uint32_t      chid = child[i];
		struct kexpr *expr = &operand[i];

		if (_expr(asts, tkns, ctx, errs, scope, chid, expr))
			goto PANIC;

		if (expr->type == JY_K_UNKNOWN) {
			uint32_t from = asts->tkns[chid];
			uint32_t to   = asts->tkns[chid];
			jry_push_error(errs, msg_inv_expression, from, to);
			goto PANIC;
		}
	}

	if (operand[0].type != operand[1].type) {
		uint32_t from = asts->tkns[child[0]];
		uint32_t to   = asts->tkns[ast];
		jry_push_error(errs, msg_operation_mismatch, from, to);
		goto PANIC;
	}

	enum jy_ast optype = asts->types[ast];
	uint8_t	    code;

	switch (optype) {
	case AST_EQUALITY:
		if (operand[0].type == JY_K_STR)
			code = JY_OP_CMPSTR;
		else
			code = JY_OP_CMP;

		expr->type = JY_K_BOOL;
		break;
	case AST_LESSER:
		if (operand[0].type != JY_K_LONG) {
			uint32_t from = asts->tkns[child[0]];
			uint32_t to   = asts->tkns[ast];
			jry_push_error(errs, msg_inv_operation, from, to);
			goto PANIC;
		}

		code	   = JY_OP_LT;
		expr->type = JY_K_BOOL;
		break;
	case AST_GREATER:
		if (operand[0].type != JY_K_LONG) {
			uint32_t from = asts->tkns[child[0]];
			uint32_t to   = asts->tkns[ast];
			jry_push_error(errs, msg_inv_operation, from, to);
			goto PANIC;
		}

		code	   = JY_OP_GT;
		expr->type = JY_K_BOOL;
		break;
	case AST_ADDITION:
		if (operand[0].type == JY_K_STR)
			code = JY_OP_CONCAT;
		else
			code = JY_OP_ADD;

		expr->type = operand[0].type;
		break;
	case AST_SUBTRACT:
		if (operand[0].type != JY_K_LONG) {
			uint32_t from = asts->tkns[child[0]];
			uint32_t to   = asts->tkns[ast];
			jry_push_error(errs, msg_inv_operation, from, to);
			goto PANIC;
		}

		code	   = JY_OP_SUB;
		expr->type = operand[0].type;
		break;
	case AST_MULTIPLY:
		if (operand[0].type != JY_K_LONG) {
			uint32_t from = asts->tkns[child[0]];
			uint32_t to   = asts->tkns[ast];
			jry_push_error(errs, msg_inv_operation, from, to);
			goto PANIC;
		}

		code	   = JY_OP_MUL;
		expr->type = operand[0].type;
		break;
	case AST_DIVIDE:
		if (operand[0].type != JY_K_LONG) {
			uint32_t from = asts->tkns[child[0]];
			uint32_t to   = asts->tkns[ast];
			jry_push_error(errs, msg_inv_operation, from, to);
			goto PANIC;
		}

		code	   = JY_OP_DIV;
		expr->type = operand[0].type;
		break;
	default: {
		uint32_t tkn = asts->tkns[ast];
		jry_push_error(errs, msg_inv_operation, tkn, tkn);
		goto PANIC;
	}
	}

	if (write_byte(code, &ctx->codes, &ctx->codesz) != 0)
		goto PANIC;

	return false;
PANIC:
	return true;
}

static cmplfn_t rules[TOTAL_AST_TYPES] = {
	[AST_EVENT]    = _event_expr,
	[AST_CALL]     = _call_expr,

	[AST_NOT]      = _not_expr,
	[AST_AND]      = _and_expr,
	[AST_OR]       = _or_expr,

	// > binaries
	[AST_EQUALITY] = _binary_expr,
	[AST_LESSER]   = _binary_expr,
	[AST_GREATER]  = _binary_expr,

	[AST_ADDITION] = _binary_expr,
	[AST_SUBTRACT] = _binary_expr,
	[AST_MULTIPLY] = _binary_expr,
	[AST_DIVIDE]   = _binary_expr,
	// < binaries

	[AST_NAME]     = _name_expr,

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
			       struct jy_errs	    *errs,
			       uint32_t		   **patchofs,
			       uint32_t		    *patchsz)
{
	uint32_t  sectkn  = asts->tkns[sect];
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t     chid  = child[i];
		uint32_t     chtkn = asts->tkns[chid];
		struct kexpr expr  = { 0 };

		if (_expr(asts, tkns, ctx, errs, ctx->names, chid, &expr))
			continue;

		if (expr.type != JY_K_BOOL) {
			jry_push_error(errs, msg_inv_match_expr, sectkn, chtkn);
			continue;
		}

		if (!find_ast_type(asts, AST_EVENT, chid)) {
			jry_push_error(errs, msg_inv_match_expr, sectkn, chtkn);
			continue;
		}

		if (write_byte(JY_OP_JMPF, &ctx->codes, &ctx->codesz) != 0)
			goto PANIC;

		jry_mem_push(*patchofs, *patchsz, ctx->codesz);

		if (patchofs == NULL)
			goto PANIC;

		*patchsz += 1;

		// Reserved 2 bytes for short jump
		if (write_byte(0, &ctx->codes, &ctx->codesz) != 0)
			goto PANIC;

		if (write_byte(0, &ctx->codes, &ctx->codesz) != 0)
			goto PANIC;
	}

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

		if (_expr(asts, tkns, ctx, errs, ctx->names, chid, &expr))
			continue;

		if (expr.type != JY_K_BOOL) {
			jry_push_error(errs, msg_inv_cond_expr, sectkn, chtkn);
			continue;
		}

		if (write_byte(JY_OP_JMPF, &ctx->codes, &ctx->codesz) != 0)
			goto PANIC;

		jry_mem_push(*patchofs, *patchsz, ctx->codesz);

		if (patchofs == NULL)
			goto PANIC;

		*patchsz += 1;

		// Reserved 2 bytes for short jump
		if (write_byte(0, &ctx->codes, &ctx->codesz) != 0)
			goto PANIC;

		if (write_byte(0, &ctx->codes, &ctx->codesz) != 0)
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

		_expr(asts, tkns, ctx, errs, ctx->names, chid, &expr);

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
			ktype = JY_K_LONG;
			break;
		case AST_STR_TYPE:
			ktype = JY_K_STR;
			break;
		default: {
			uint32_t from = asts->tkns[id];
			uint32_t to   = asts->tkns[op_id];
			jry_push_error(errs, msg_inv_type, from, to);
			goto PANIC;
		}
		}

		int e = jry_add_def(def, name, (jy_val_t) NULL, ktype);

		if (e != ERROR_SUCCESS)
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
	bool panic	   = false;

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

	if (matchsz == 0) {
		jry_push_error(errs, msg_missing_match_sect, ruletkn, ruletkn);
		goto PANIC;
	}

	if (targetsz == 0) {
		jry_push_error(errs, msg_missing_target_sect, ruletkn, ruletkn);
		goto PANIC;
	}

	for (uint32_t i = 0; i < matchsz; ++i) {
		uint32_t id = matchs[i];
		_match_sect(asts, tkns, id, ctx, errs, &patchofs, &patchsz);
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

	goto END;

PANIC:
	panic = true;
END:
	jry_free(patchofs);
	return panic;
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

	char *lex	  = tkns->lexemes[asts->tkns[id]];

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

	struct jy_defs *def = defobj(&ctx->obj);

	if (def == NULL)
		goto PANIC;

	for (uint32_t i = 0; i < fieldsz; ++i)
		if (_field_sect(asts, tkns, errs, fields[i], def))
			goto PANIC;

	jy_val_t offset = jry_long2v(memory_offset(ctx->obj.buf, def));

	if (write_constant(offset, JY_K_EVENT, &ctx->vals, &ctx->types,
			   &ctx->valsz) != 0)
		goto PANIC;

	if (jry_add_def(ctx->names, lex, offset, JY_K_EVENT) != 0)
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
	int	       errcode = jry_module_load(path, &def, &ctx->obj);

	if (errcode != 0) {
		const char *msg = jry_module_error(errcode);
		jry_push_error(errs, msg, tkn, tkn);
		goto PANIC;
	}

	struct jy_defs *module = defobj(&ctx->obj);
	*module		       = def;
	jy_val_t value = jry_long2v(memory_offset(ctx->obj.buf, module));

	if (jry_add_def(ctx->names, lexeme, value, JY_K_MODULE) != 0)
		goto PANIC;

	if (write_constant(value, JY_K_MODULE, &ctx->vals, &ctx->types,
			   &ctx->valsz) != 0)
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

	if (write_byte(JY_OP_END, &ctx->codes, &ctx->codesz) != 0)
		return true;

	return false;
}

void jry_compile(const struct jy_asts *asts,
		 const struct jy_tkns *tkns,
		 struct jy_jay	      *ctx,
		 struct jy_errs	      *errs)
{
	jry_assert(ctx->names != NULL);

	uint32_t    root = 0;
	enum jy_ast type = asts->types[root];

	if (type != AST_ROOT)
		return;

	_root(asts, tkns, ctx, errs, root);
}

int jry_set_event(const char	       *event,
		  const char	       *field,
		  jy_val_t		value,
		  const void	       *buf,
		  const struct jy_defs *names)
{
	uint32_t id;

	if (!jry_find_def(names, event, &id))
		return 1;

	struct jy_defs *ev = memory_fetch(buf, jry_v2long(names->vals[id]));

	if (!jry_find_def(ev, field, &id))
		return 2;

	ev->vals[id] = value;

	return 0;
}

void jry_free_jay(struct jy_jay ctx)
{
	for (uint32_t i = 0; i < ctx.valsz; ++i) {
		jy_val_t v   = ctx.vals[i];
		uint32_t ofs = jry_v2long(v);

		switch (ctx.types[i]) {
		case JY_K_MODULE: {
			struct jy_defs *module = memory_fetch(ctx.obj.buf, ofs);
			jry_module_unload(module);
			break;
		}
		case JY_K_EVENT: {
			struct jy_defs *ev = memory_fetch(ctx.obj.buf, ofs);
			jry_free_def(*ev);
			break;
		}
		default:
			break;
		}
	}

	jry_free(ctx.codes);
	jry_free(ctx.vals);
	jry_free(ctx.types);
	jry_free(ctx.obj.buf);

	if (ctx.names)
		jry_free_def(*ctx.names);
}
