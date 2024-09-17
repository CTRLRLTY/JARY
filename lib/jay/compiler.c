#include "compiler.h"

#include "dload.h"

#include "jary/error.h"
#include "jary/memory.h"
#include "jary/object.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char msg_not_a_literal[]	    = "not a literal";
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
static const char msg_operation_mismatch[]  = "invalid operation type mismatch";
static const char msg_argument_mismatch[]   = "bad argument type mismatch";
static const char msg_type_mismatch[]	    = "type mismatch";
static const char msg_redefinition[]	    = "redefinition of";
static const char msg_call_capped[]	    = "Too many function definitons...";
static const char msg_event_capped[]	    = "Too many event definitions...";

struct kexpr {
	uint32_t      id;
	enum jy_ktype type;
};

// expression compile subroutine signature
typedef bool (*cmplfn_t)(struct jy_asts *,
			 struct jy_tkns *,
			 struct jy_scan_ctx *,
			 struct jy_errs *,
			 struct jy_defs *,
			 uint32_t,
			 struct kexpr *);

static inline cmplfn_t rule_expression(enum jy_ast type);

static bool isnum(enum jy_ktype type)
{
	switch (type) {
	case JY_K_LONG:
		return true;
	default:
		return false;
	}
}

static bool find_ast_type(struct jy_asts *asts, enum jy_ast type, uint32_t from)
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

static inline void write_byte(uint8_t **codes, uint32_t *codesz, uint8_t code)
{
	jry_mem_push(*codes, *codesz, code);

	*codesz += 1;
}

static void write_push(uint8_t **code, uint32_t *codesz, uint32_t constant)
{
	uint8_t width = 0;

	union {
		uint32_t num;
		char	 c[sizeof(constant)];
	} v;

	v.num = constant;

	if (constant <= 0xff) {
		write_byte(code, codesz, JY_OP_PUSH8);
		width = 1;
	} else if (constant <= 0xffff) {
		write_byte(code, codesz, JY_OP_PUSH16);
		width = 2;
	} else if (constant <= 0xffffffff) {
		write_byte(code, codesz, JY_OP_PUSH32);
		width = 4;
	} else {
		write_byte(code, codesz, JY_OP_PUSH64);
		width = 8;
	}

	for (uint8_t i = 0; i < width; ++i)
		write_byte(code, codesz, v.c[i]);
}

static inline bool cmplexpr(struct jy_asts     *asts,
			    struct jy_tkns     *tkns,
			    struct jy_scan_ctx *ctx,
			    struct jy_errs     *errs,
			    struct jy_defs     *scope,
			    uint32_t		id,
			    struct kexpr       *expr)
{
	enum jy_ast type = asts->types[id];
	cmplfn_t    fn	 = rule_expression(type);

	return fn(asts, tkns, ctx, errs, scope, id, expr);
}

static bool cmplliteral(struct jy_asts	   *asts,
			struct jy_tkns	   *tkns,
			struct jy_scan_ctx *ctx,
			struct jy_errs	   *errs,
			struct jy_defs	   *__unused(scope),
			uint32_t	    id,
			struct kexpr	   *expr)
{
	enum jy_ast type = asts->types[id];

	uint32_t tkn	 = asts->tkns[id];
	char	*lexeme	 = tkns->lexemes[tkn];
	uint32_t lexsz	 = tkns->lexsz[tkn];

	switch (type) {
	case AST_LONG: {
		long num   = strtol(lexeme, NULL, 10);
		expr->type = JY_K_LONG;

		for (uint32_t i = 0; i < ctx->valsz; ++i) {
			enum jy_ktype t = ctx->types[i];
			long	      v = jry_v2long(ctx->vals[i]);

			if (t != JY_K_LONG || v != num)
				continue;

			expr->id = i;

			goto DONE;
		}

		jy_val_t val = jry_long2v(num);

		jry_mem_push(ctx->vals, ctx->valsz, val);
		jry_mem_push(ctx->types, ctx->valsz, JY_K_LONG);

		if (ctx->vals == NULL || ctx->types == NULL)
			goto PANIC;

		expr->id    = ctx->valsz;
		ctx->valsz += 1;

		break;
	}
	case AST_STRING: {
		expr->type = JY_K_STR;

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

		uint32_t moffs1 = sizeof(struct jy_obj_str);
		uint32_t moffs2 = moffs1 + lexsz;
		ctx->obj	= jry_realloc(ctx->obj, moffs2);

		if (ctx->obj == NULL)
			goto PANIC;

		char		  *mem	= ((char *) ctx->obj) + ctx->objsz;
		struct jy_obj_str *ostr = (void *) mem;
		ostr->str		= (void *) (mem + moffs1);
		ostr->size		= lexsz;
		ctx->objsz		= moffs2;

		memcpy(ostr->str, lexeme, lexsz);

		jry_mem_push(ctx->vals, ctx->valsz, jry_str2v(ostr));
		jry_mem_push(ctx->types, ctx->valsz, JY_K_STR);

		if (ctx->vals == NULL || ctx->types == NULL)
			goto PANIC;

		expr->id    = ctx->valsz;
		ctx->valsz += 1;

		break;
	}
	default:
		jry_push_error(errs, msg_not_a_literal, tkn, tkn);
		goto PANIC;
	}

DONE:
	write_push(&ctx->codes, &ctx->codesz, expr->id);
	return false;

PANIC:
	return true;
}

static bool cmplfield(struct jy_asts *asts,
		      struct jy_tkns *tkns,
		      struct jy_errs *errs,
		      struct jy_defs *scope,
		      uint32_t	      id,
		      struct kexpr   *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];
	uint32_t lexsz	= tkns->lexsz[tkn];

	uint32_t nid;

	if (!jry_find_def(scope, lexeme, lexsz, &nid)) {
		jry_push_error(errs, msg_no_definition, tkn, tkn);
		goto PANIC;
	}

	expr->id   = nid;
	expr->type = scope->types[nid];

	return false;
PANIC:
	return true;
}

static bool cmplname(struct jy_asts	*asts,
		     struct jy_tkns	*tkns,
		     struct jy_scan_ctx *ctx,
		     struct jy_errs	*errs,
		     struct jy_defs	*scope,
		     uint32_t		 id,
		     struct kexpr	*expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];
	uint32_t lexsz	= tkns->lexsz[tkn];

	uint32_t nid;

	if (!jry_find_def(scope, lexeme, lexsz, &nid)) {
		jry_push_error(errs, msg_no_definition, tkn, tkn);
		goto PANIC;
	}

	struct jy_defs *def	= jry_v2def(scope->vals[nid]);
	uint32_t       *child	= asts->child[id];
	uint32_t	childsz = asts->childsz[id];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t chid = child[i];

		if (cmplexpr(asts, tkns, ctx, errs, def, chid, expr))
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static bool cmplevent(struct jy_asts	 *asts,
		      struct jy_tkns	 *tkns,
		      struct jy_scan_ctx *ctx,
		      struct jy_errs	 *errs,
		      struct jy_defs	 *__unused(scope),
		      uint32_t		  id,
		      struct kexpr	 *expr)
{
	uint32_t tkn	= asts->tkns[id];
	char	*lexeme = tkns->lexemes[tkn];
	uint32_t lexsz	= tkns->lexsz[tkn];

	uint32_t nid;

	if (!jry_find_def(ctx->names, lexeme, lexsz, &nid)) {
		jry_push_error(errs, msg_no_definition, tkn, tkn);
		goto PANIC;
	}

	struct jy_defs *def   = jry_v2def(ctx->names->vals[nid]);
	uint32_t       *child = asts->child[id];

	jry_assert(asts->childsz[id] == 1);

	uint32_t     chid    = child[0];
	struct kexpr operand = { 0 };

	if (cmplfield(asts, tkns, errs, def, chid, &operand))
		goto PANIC;

	uint32_t event = 0;

	for (uint32_t i = 0; i < ctx->eventsz; ++i) {
		struct jy_defs *temp = &ctx->events[i];

		if (temp == def) {
			event = i;
			break;
		}
	}

	uint32_t kid;

	for (uint32_t i = 0; i < ctx->valsz; ++i) {
		enum jy_ktype t = ctx->types[i];

		if (t != JY_K_EVENT)
			continue;

		struct jy_obj_event ev = jry_v2event(ctx->vals[i]);

		if (ev.event != event || ev.name != operand.id)
			continue;

		kid = i;

		goto EMIT;
	}

	struct jy_obj_event ev	= { .event = event, .name = operand.id };
	jy_val_t	    val = jry_event2v(ev);

	jry_mem_push(ctx->vals, ctx->valsz, val);
	jry_mem_push(ctx->types, ctx->valsz, JY_K_EVENT);

	if (ctx->types == NULL)
		goto PANIC;

	kid	    = ctx->valsz;
	ctx->valsz += 1;

EMIT:
	expr->id   = kid;
	expr->type = JY_K_EVENT;
	write_push(&ctx->codes, &ctx->codesz, kid);

	return false;
PANIC:
	return true;
}

static bool cmplcall(struct jy_asts	*asts,
		     struct jy_tkns	*tkns,
		     struct jy_scan_ctx *ctx,
		     struct jy_errs	*errs,
		     struct jy_defs	*scope,
		     uint32_t		 ast,
		     struct kexpr	*expr)
{
	uint32_t tkn	= asts->tkns[ast];
	char	*lexeme = tkns->lexemes[tkn];
	uint32_t lexsz	= tkns->lexsz[tkn];

	uint32_t nid;

	if (!jry_find_def(scope, lexeme, lexsz, &nid)) {
		jry_push_error(errs, msg_no_definition, tkn, tkn);
		goto PANIC;
	}

	enum jy_ktype type = scope->types[nid];

	if (type != JY_K_FUNC) {
		jry_push_error(errs, msg_type_mismatch, tkn, tkn);
		goto PANIC;
	}

	jy_val_t	    val	  = scope->vals[nid];
	struct jy_obj_func *ofunc = jry_v2func(val);

	uint32_t *child		  = asts->child[ast];
	uint32_t  childsz	  = asts->childsz[ast];

	if (childsz != ofunc->param_sz) {
		jry_push_error(errs, msg_inv_signature, tkn, tkn);
		goto PANIC;
	}

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t      chid   = child[i];
		enum jy_ktype expect = ofunc->param_types[i];
		struct kexpr  pexpr  = { 0 };

		if (cmplexpr(asts, tkns, ctx, errs, scope, chid, &pexpr))
			goto PANIC;

		if (expect != pexpr.type) {
			uint32_t chtkn = asts->tkns[chid];
			jry_push_error(errs, msg_argument_mismatch, tkn, chtkn);
			goto PANIC;
		}
	}

	uint32_t call_id;

	for (uint32_t i = 0; i < ctx->callsz; ++i) {
		jy_funcptr_t f = ctx->call[i];

		if (f != ofunc->func)
			continue;

		call_id = i;
		goto EMIT;
	}

	if ((ctx->callsz + 1) & 0x10000) {
		jry_push_error(errs, msg_call_capped, tkn, tkn);
		goto PANIC;
	}

	jry_mem_push(ctx->call, ctx->callsz, ofunc->func);

	if (ctx->call == NULL)
		goto PANIC;

	call_id	     = ctx->callsz;
	ctx->callsz += 1;

EMIT:
	write_byte(&ctx->codes, &ctx->codesz, JY_OP_CALL);
	write_byte(&ctx->codes, &ctx->codesz, ofunc->param_sz);
	write_byte(&ctx->codes, &ctx->codesz, call_id & 0xFF00);
	write_byte(&ctx->codes, &ctx->codesz, call_id & 0x00FF);

	expr->type = ofunc->return_type;

	return false;
PANIC:
	return true;
}

static bool cmplbinary(struct jy_asts	  *asts,
		       struct jy_tkns	  *tkns,
		       struct jy_scan_ctx *ctx,
		       struct jy_errs	  *errs,
		       struct jy_defs	  *scope,
		       uint32_t		   ast,
		       struct kexpr	  *expr)
{
	jry_assert(asts->childsz[ast] == 2);

	uint32_t    *child	= asts->child[ast];
	struct kexpr operand[2] = { 0 };

	for (uint32_t i = 0; i < 2; ++i) {
		uint32_t      chid = child[i];
		struct kexpr *expr = &operand[i];

		if (cmplexpr(asts, tkns, ctx, errs, scope, chid, expr))
			goto PANIC;

		switch (expr->type) {
		case JY_K_EVENT: {
			jy_val_t	    v	= ctx->vals[expr->id];
			struct jy_obj_event ev	= jry_v2event(v);
			struct jy_defs	    def = ctx->events[ev.event];
			expr->type		= def.types[ev.name];
			break;
		}
		case JY_K_UNKNOWN: {
			uint32_t from = asts->tkns[chid];
			uint32_t to   = asts->tkns[ast];
			jry_push_error(errs, msg_inv_operation, from, to);
			goto PANIC;
		}
		default:
			break;
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
		code	   = JY_OP_CMP;
		expr->type = JY_K_BOOL;
		break;
	case AST_LESSER:
		if (!isnum(operand[0].type)) {
			uint32_t from = asts->tkns[child[0]];
			uint32_t to   = asts->tkns[ast];
			jry_push_error(errs, msg_inv_operation, from, to);
			goto PANIC;
		}
		code	   = JY_OP_LT;
		expr->type = JY_K_BOOL;
		break;
	case AST_GREATER:
		if (!isnum(operand[0].type)) {
			uint32_t from = asts->tkns[child[0]];
			uint32_t to   = asts->tkns[ast];
			jry_push_error(errs, msg_inv_operation, from, to);
			goto PANIC;
		}
		code	   = JY_OP_GT;
		expr->type = JY_K_BOOL;
		break;
	case AST_ADDITION:
		code	   = JY_OP_ADD;
		expr->type = operand[0].type;
		break;
	case AST_SUBTRACT:
		code	   = JY_OP_SUB;
		expr->type = operand[0].type;
		break;
	case AST_MULTIPLY:
		code	   = JY_OP_MUL;
		expr->type = operand[0].type;
		break;
	case AST_DIVIDE:
		code	   = JY_OP_DIV;
		expr->type = operand[0].type;
		break;
	default: {
		uint32_t tkn = asts->tkns[ast];
		jry_push_error(errs, msg_inv_operation, tkn, tkn);
		goto PANIC;
	}
	}

	write_byte(&ctx->codes, &ctx->codesz, code);

	return false;
PANIC:
	return true;
}

static cmplfn_t rules[TOTAL_AST_TYPES] = {
	[AST_EVENT]    = cmplevent,
	[AST_CALL]     = cmplcall,

	// > binaries
	[AST_EQUALITY] = cmplbinary,
	[AST_LESSER]   = cmplbinary,
	[AST_GREATER]  = cmplbinary,

	[AST_ADDITION] = cmplbinary,
	[AST_SUBTRACT] = cmplbinary,
	[AST_MULTIPLY] = cmplbinary,
	[AST_DIVIDE]   = cmplbinary,
	// < binaries

	[AST_NAME]     = cmplname,

	// > literal
	[AST_REGEXP]   = cmplliteral,
	[AST_LONG]     = cmplliteral,
	[AST_STRING]   = cmplliteral,
	[AST_FALSE]    = cmplliteral,
	[AST_TRUE]     = cmplliteral,
	// < literal
};

static inline cmplfn_t rule_expression(enum jy_ast type)
{
	jry_assert(rules[type] != NULL);

	return rules[type];
}

static inline bool cmplmatchsect(struct jy_asts	    *asts,
				 struct jy_tkns	    *tkns,
				 struct jy_scan_ctx *ctx,
				 struct jy_errs	    *errs,
				 uint32_t	     sect,
				 uint32_t	   **patchofs,
				 uint32_t	    *patchsz)
{
	uint32_t  sectkn  = asts->tkns[sect];
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t     chid  = child[i];
		uint32_t     chtkn = asts->tkns[chid];
		struct kexpr expr  = { 0 };

		if (cmplexpr(asts, tkns, ctx, errs, ctx->names, chid, &expr))
			continue;

		if (expr.type != JY_K_BOOL) {
			jry_push_error(errs, msg_inv_match_expr, sectkn, chtkn);
			continue;
		}

		if (!find_ast_type(asts, AST_EVENT, chid)) {
			jry_push_error(errs, msg_inv_match_expr, sectkn, chtkn);
			continue;
		}

		write_byte(&ctx->codes, &ctx->codesz, JY_OP_JMPF);

		jry_mem_push(*patchofs, *patchsz, ctx->codesz);

		if (patchofs == NULL)
			goto PANIC;

		*patchsz += 1;

		// Reserved 2 bytes for short jump
		write_byte(&ctx->codes, &ctx->codesz, 0);
		write_byte(&ctx->codes, &ctx->codesz, 0);
	}

	return false;
PANIC:
	return true;
}

static inline bool cmplcondsect(struct jy_asts	   *asts,
				struct jy_tkns	   *tkns,
				struct jy_scan_ctx *ctx,
				struct jy_errs	   *errs,
				uint32_t	    sect,
				uint32_t	  **patchofs,
				uint32_t	   *patchsz)
{
	uint32_t  sectkn  = asts->tkns[sect];
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t     chid  = child[i];
		uint32_t     chtkn = asts->tkns[chid];
		struct kexpr expr  = { 0 };

		if (cmplexpr(asts, tkns, ctx, errs, ctx->names, chid, &expr))
			continue;

		if (expr.type != JY_K_BOOL) {
			jry_push_error(errs, msg_inv_cond_expr, sectkn, chtkn);
			continue;
		}

		write_byte(&ctx->codes, &ctx->codesz, JY_OP_JMPF);

		jry_mem_push(*patchofs, *patchsz, ctx->codesz);

		if (patchofs == NULL)
			goto PANIC;

		*patchsz += 1;

		// Reserved 2 bytes for short jump
		write_byte(&ctx->codes, &ctx->codesz, 0);
		write_byte(&ctx->codes, &ctx->codesz, 0);
	}

	return false;
PANIC:
	return true;
}

static inline bool cmpltargetsect(struct jy_asts     *asts,
				  struct jy_tkns     *tkns,
				  struct jy_scan_ctx *ctx,
				  struct jy_errs     *errs,
				  uint32_t	      sect)
{
	uint32_t *child	  = asts->child[sect];
	uint32_t  childsz = asts->childsz[sect];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t     chid = child[i];
		struct kexpr expr = { 0 };

		cmplexpr(asts, tkns, ctx, errs, ctx->names, chid, &expr);

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

static inline bool cmplfieldsect(struct jy_asts *asts,
				 struct jy_tkns *tkns,
				 struct jy_errs *errs,
				 uint32_t	 id,
				 struct jy_defs *def)
{
	uint32_t *child	  = asts->child[id];
	uint32_t  childsz = asts->childsz[id];

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t op_id	 = child[i];
		uint32_t name_id = asts->child[op_id][0];
		uint32_t type_id = asts->child[op_id][1];
		char	*name	 = tkns->lexemes[asts->tkns[name_id]];
		uint32_t length	 = tkns->lexsz[asts->tkns[name_id]];

		if (jry_find_def(def, name, length, NULL)) {
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

		int e = jry_add_def(def, name, length, (jy_val_t) NULL, ktype);

		if (e != ERROR_SUCCESS)
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static inline bool cmplruledecl(struct jy_asts	   *asts,
				struct jy_tkns	   *tkns,
				struct jy_scan_ctx *ctx,
				struct jy_errs	   *errs,
				uint32_t	    rule)
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
		cmplmatchsect(asts, tkns, ctx, errs, id, &patchofs, &patchsz);
	}

	for (uint32_t i = 0; i < condsz; ++i) {
		uint32_t id = conds[i];
		cmplcondsect(asts, tkns, ctx, errs, id, &patchofs, &patchsz);
	}

	for (uint32_t i = 0; i < targetsz; ++i) {
		uint32_t id = targets[i];
		cmpltargetsect(asts, tkns, ctx, errs, id);
	}

	uint32_t endofs = ctx->codesz;

	for (uint32_t i = 0; i < patchsz; ++i) {
		uint32_t ofs = patchofs[i];
		short	 jmp = (short) (endofs - ofs + 1);

		memcpy(ctx->codes + ofs, &jmp, sizeof(jmp));
	}

	goto END;

PANIC:
	panic = true;
END:
	jry_free(patchofs);
	return panic;
}

static inline bool cmplingressdecl(struct jy_asts     *asts,
				   struct jy_tkns     *tkns,
				   struct jy_scan_ctx *ctx,
				   struct jy_errs     *errs,
				   uint32_t	       id)
{
	uint32_t  tkn	  = asts->tkns[id];
	uint32_t *child	  = asts->child[id];
	uint32_t  childsz = asts->childsz[id];

	char	*lex	  = tkns->lexemes[asts->tkns[id]];
	uint32_t lexsz	  = tkns->lexsz[asts->tkns[id]];

	uint32_t fields[childsz];
	uint32_t fieldsz = 0;

	if (jry_find_def(ctx->names, lex, lexsz, NULL)) {
		jry_push_error(errs, msg_redefinition, tkn, tkn);
		goto PANIC;
	}

	if (ctx->eventsz & 0x10000) {
		jry_push_error(errs, msg_event_capped, tkn, tkn);
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

	struct jy_defs def = { .keys = NULL };

	for (uint32_t i = 0; i < fieldsz; ++i)
		cmplfieldsect(asts, tkns, errs, fields[i], &def);

	uint32_t oldsz	 = ctx->eventsz;
	uint32_t allocsz = sizeof(def) * (oldsz + 1);
	ctx->events	 = jry_realloc(ctx->events, allocsz);

	if (ctx->events == NULL)
		goto PANIC;

	ctx->events[oldsz] = def;
	jy_val_t val	   = jry_def2v(&ctx->events[oldsz]);

	if (jry_add_def(ctx->names, lex, lexsz, val, JY_K_EVENT) != 0)
		goto PANIC;

	ctx->eventsz += 1;

	return false;

PANIC:
	return true;
}

static inline bool cmplimportstmt(struct jy_asts     *asts,
				  struct jy_tkns     *tkns,
				  struct jy_scan_ctx *ctx,
				  struct jy_errs     *errs,
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

	int module = jry_module_load(path);

	if (module < 0) {
		const char *msg = jry_module_error(module);
		jry_push_error(errs, msg, tkn, tkn);
		return true;
	}

	struct jy_defs *def = jry_module_def(module);

	jry_mem_push(ctx->modules, ctx->modulesz, module);
	ctx->modulesz += 1;

	jy_val_t v     = jry_def2v(def);

	if (jry_add_def(ctx->names, lexeme, lexsz, v, JY_K_MODULE) != 0)
		return true;

	return false;
}

static inline bool cmplroot(struct jy_asts     *asts,
			    struct jy_tkns     *tkns,
			    struct jy_scan_ctx *ctx,
			    struct jy_errs     *errs,
			    uint32_t		id)
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
		cmplimportstmt(asts, tkns, ctx, errs, imports[i]);

	for (uint32_t i = 0; i < ingress_sz; ++i)
		cmplingressdecl(asts, tkns, ctx, errs, ingress[i]);

	for (uint32_t i = 0; i < rules_sz; ++i)
		cmplruledecl(asts, tkns, ctx, errs, rules[i]);

	write_byte(&ctx->codes, &ctx->codesz, JY_OP_END);

	return false;
}

void jry_compile(struct jy_asts	    *asts,
		 struct jy_tkns	    *tkns,
		 struct jy_scan_ctx *ctx,
		 struct jy_errs	    *errs)
{
	jry_assert(ctx->names != NULL);

	uint32_t    root = 0;
	enum jy_ast type = asts->types[root];

	if (type != AST_ROOT)
		return;

	cmplroot(asts, tkns, ctx, errs, root);
}

void jry_free_scan_ctx(struct jy_scan_ctx ctx)
{
	for (uint32_t i = 0; i < ctx.modulesz; ++i)
		jry_module_unload(ctx.modules[i]);

	jry_free(ctx.modules);
	jry_free(ctx.codes);

	jry_free(ctx.vals);
	jry_free(ctx.types);
	jry_free(ctx.obj);
	jry_free(ctx.call);

	for (uint32_t i = 0; i < ctx.eventsz; ++i)
		jry_free_def(ctx.events[i]);

	jry_free(ctx.events);

	if (ctx.names)
		jry_free_def(*ctx.names);
}
