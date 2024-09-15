#include "compiler.h"

#include "jary/error.h"
#include "jary/memory.h"
#include "jary/object.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static char msg_not_a_literal[]	      = "not a literal";
static char msg_no_definition[]	      = "undefined";
static char msg_missing_match_sect[]  = "missing match section";
static char msg_missing_target_sect[] = "missing target section";
static char msg_inv_type[]	      = "invalid type";
static char msg_inv_signature[]	      = "invalid signature";
static char msg_inv_rule_sect[]	      = "invalid rule section";
static char msg_inv_match_expr[]      = "invalid match expression";
static char msg_inv_target_expr[]     = "invalid target expression";
static char msg_inv_operation[]	      = "invalid operation";
static char msg_type_mismatch[]	      = "type mismatch";
static char msg_redefinition[]	      = "redefinition";

struct kexpr {
	size_t	      id;
	enum jy_ktype type;
};

// expression compile subroutine signature
typedef bool (*cmplfn_t)(struct jy_asts *,
			 struct jy_tkns *,
			 struct jy_scan_ctx *,
			 struct jy_errs *,
			 struct jy_defs *,
			 size_t,
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

static bool find_ast_type(struct jy_asts *asts,
			  enum jy_ast	  type,
			  size_t	  from,
			  size_t	 *end)
{
	enum jy_ast current_type = asts->types[from];

	if (current_type == type)
		return true;

	size_t *child	= asts->child[from];
	size_t	childsz = asts->childsz[from];

	for (uint32_t i = 0; i < childsz; ++i) {
		size_t chid = child[i];

		if (find_ast_type(asts, type, chid, end)) {
			*end = chid;
			return true;
		}
	}

	*end = from;
	return false;
}

static int push_err(struct jy_errs *errs, const char *msg, uint32_t astid)
{
	jry_mem_push(errs->msgs, errs->size, msg);
	jry_mem_push(errs->ids, errs->size, astid);

	if (errs->ids == NULL)
		return ERROR_NOMEM;

	errs->size += 1;

	return ERROR_SUCCESS;
}

static inline void write_byte(struct jy_chunks *cnk, uint8_t code)
{
	jry_mem_push(cnk->codes, cnk->size, code);

	cnk->size += 1;
}

static void write_push(struct jy_chunks *cnk, size_t constant)
{
	uint8_t width = 0;

	union {
		size_t num;
		char   c[sizeof(constant)];
	} v;

	v.num = constant;

	if (constant <= 0xff) {
		write_byte(cnk, JY_OP_PUSH8);
		width = 1;
	} else if (constant <= 0xffff) {
		write_byte(cnk, JY_OP_PUSH16);
		width = 2;
	} else if (constant <= 0xffffffff) {
		write_byte(cnk, JY_OP_PUSH32);
		width = 4;
	} else {
		write_byte(cnk, JY_OP_PUSH64);
		width = 8;
	}

	for (uint8_t i = 0; i < width; ++i)
		write_byte(cnk, v.c[i]);
}

static inline bool find_longk(struct jy_kpool *pool, long num, size_t *id)
{
	for (size_t i = 0; i < pool->size; ++i) {
		enum jy_ktype t = pool->types[i];

		if (t != JY_K_LONG)
			continue;

		long v = jry_v2long(pool->vals[i]);

		if (v != num)
			continue;

		if (id != NULL)
			*id = i;

		return true;
	}

	return false;
}

static inline bool find_strk(struct jy_kpool *pool,
			     const char	     *str,
			     size_t	      length,
			     size_t	     *id)
{
	for (size_t i = 0; i < pool->size; ++i) {
		enum jy_ktype t = pool->types[i];

		if (t != JY_K_STR)
			continue;

		struct jy_obj_str *v = jry_v2str(pool->vals[i]);

		if (v->size != length)
			continue;

		if (v->str[1] != str[1])
			continue;

		if (memcmp(v->str + 1, str + 1, v->size - 1) != 0)
			continue;

		if (id != NULL)
			*id = i;

		return true;
	}

	return false;
}

static inline bool find_funck(struct jy_kpool *pool,
			      jy_funcptr_t     fn,
			      size_t	      *id)
{
	for (size_t i = 0; i < pool->size; ++i) {
		enum jy_ktype t = pool->types[i];

		if (t != JY_K_FUNC)
			continue;

		jy_funcptr_t f = jry_v2func(pool->vals[i])->func;

		if (f != fn)
			continue;

		if (id != NULL)
			*id = i;

		return true;
	}

	return false;
}

USE_RESULT static inline int add_longk(struct jy_kpool *pool,
				       long		num,
				       size_t	       *id)
{
	jy_val_t val = jry_long2v(num);

	jry_mem_push(pool->vals, pool->size, val);
	jry_mem_push(pool->types, pool->size, JY_K_LONG);

	if (pool->vals == NULL || pool->types == NULL)
		return ERROR_NOMEM;

	*id = pool->size++;

	return ERROR_SUCCESS;
}

USE_RESULT static inline int add_strk(struct jy_kpool *pool,
				      const char      *str,
				      size_t	       length,
				      size_t	      *id)
{
	uint32_t moffs1	 = sizeof(struct jy_obj_str);
	uint32_t moffs2	 = moffs1 + sizeof(length);
	size_t	 objsz	 = pool->objsz;

	pool->objsz	+= moffs2;
	pool->obj	 = jry_realloc(pool->obj, pool->objsz);

	if (pool->obj == NULL)
		return ERROR_NOMEM;

	char		  *mem	= ((char *) pool->obj) + objsz;
	struct jy_obj_str *ostr = (void *) mem;

	ostr->str		= (void *) (mem + moffs1);
	ostr->size		= length;

	memcpy(ostr->str, str, length);

	jry_mem_push(pool->vals, pool->size, jry_str2v(ostr));
	jry_mem_push(pool->types, pool->size, JY_K_STR);

	if (pool->vals == NULL || pool->types == NULL)
		return ERROR_NOMEM;

	*id = pool->size++;

	return ERROR_SUCCESS;
}

USE_RESULT static inline int add_funck(struct jy_kpool	  *pool,
				       struct jy_obj_func *func,
				       size_t		  *id)
{
	jry_mem_push(pool->vals, pool->size, jry_func2v(func));

	if (pool->vals == NULL)
		return ERROR_NOMEM;

	jry_mem_push(pool->types, pool->size, JY_K_FUNC);

	if (pool->types == NULL)
		return ERROR_NOMEM;

	*id = pool->size++;

	return ERROR_SUCCESS;
}

static inline bool cmplexpr(struct jy_asts     *asts,
			    struct jy_tkns     *tkns,
			    struct jy_scan_ctx *ctx,
			    struct jy_errs     *errs,
			    struct jy_defs     *scope,
			    size_t		id,
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
			struct jy_defs	   *UNUSED(scope),
			size_t		    id,
			struct kexpr	   *expr)
{
	enum jy_ast type = asts->types[id];

	size_t tkn	 = asts->tkns[id];
	char  *lexeme	 = tkns->lexemes[tkn];
	size_t lexsz	 = tkns->lexsz[tkn];

	switch (type) {
	case AST_LONG: {
		long num   = strtol(lexeme, NULL, 10);

		expr->type = JY_K_LONG;

		if (find_longk(ctx->pool, num, &expr->id))
			break;

		int status = add_longk(ctx->pool, num, &expr->id);

		if (status != ERROR_SUCCESS)
			goto PANIC;

		break;
	}
	case AST_STRING: {
		expr->type = JY_K_STR;

		if (find_strk(ctx->pool, lexeme, lexsz, &expr->id))
			break;

		int status = add_strk(ctx->pool, lexeme, lexsz, &expr->id);

		if (status != ERROR_SUCCESS)
			goto PANIC;

		break;
	}
	default:
		push_err(errs, msg_not_a_literal, id);
		goto PANIC;
	}

	write_push(ctx->cnk, expr->id);

	return false;

PANIC:
	return true;
}

static bool cmplfield(struct jy_asts	 *asts,
		      struct jy_tkns	 *tkns,
		      struct jy_scan_ctx *ctx,
		      struct jy_errs	 *errs,
		      struct jy_defs	 *scope,
		      size_t		  id,
		      struct kexpr	 *expr)
{
	size_t tkn    = asts->tkns[id];
	char  *lexeme = tkns->lexemes[tkn];
	size_t lexsz  = tkns->lexsz[tkn];

	size_t nid;

	if (!jry_find_def(scope, lexeme, lexsz, &nid)) {
		push_err(errs, msg_no_definition, id);
		goto PANIC;
	}

	write_push(ctx->cnk, nid);
	enum jy_ktype type = scope->types[nid];
	expr->type	   = type;

	return false;
PANIC:
	return true;
}

static bool cmplname(struct jy_asts	*asts,
		     struct jy_tkns	*tkns,
		     struct jy_scan_ctx *ctx,
		     struct jy_errs	*errs,
		     struct jy_defs	*scope,
		     size_t		 id,
		     struct kexpr	*expr)
{
	size_t tkn    = asts->tkns[id];
	char  *lexeme = tkns->lexemes[tkn];
	size_t lexsz  = tkns->lexsz[tkn];

	size_t nid;

	if (!jry_find_def(scope, lexeme, lexsz, &nid)) {
		push_err(errs, msg_no_definition, id);
		goto PANIC;
	}

	struct jy_defs *def	= jry_v2def(scope->vals[nid]);
	size_t	       *child	= asts->child[id];
	size_t		childsz = asts->childsz[id];

	for (size_t i = 0; i < childsz; ++i) {
		size_t chid = child[i];

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
		      struct jy_defs	 *UNUSED(scope),
		      size_t		  id,
		      struct kexpr	 *expr)
{
	if (cmplname(asts, tkns, ctx, errs, ctx->names, id, expr))
		goto PANIC;

	write_byte(ctx->cnk, JY_OP_EVENT);

	return false;
PANIC:
	return true;
}

static bool cmplcall(struct jy_asts	*asts,
		     struct jy_tkns	*tkns,
		     struct jy_scan_ctx *ctx,
		     struct jy_errs	*errs,
		     struct jy_defs	*scope,
		     size_t		 id,
		     struct kexpr	*expr)
{
	size_t tkn    = asts->tkns[id];
	char  *lexeme = tkns->lexemes[tkn];
	size_t lexsz  = tkns->lexsz[tkn];

	size_t nid;

	if (!jry_find_def(scope, lexeme, lexsz, &nid)) {
		push_err(errs, msg_no_definition, id);
		goto PANIC;
	}

	enum jy_ktype type = scope->types[nid];

	if (type != JY_K_FUNC) {
		push_err(errs, msg_type_mismatch, id);
		goto PANIC;
	}

	jy_val_t	    val	  = scope->vals[nid];
	struct jy_obj_func *ofunc = jry_v2func(val);

	size_t *child		  = asts->child[id];
	size_t	childsz		  = asts->childsz[id];

	if (childsz != ofunc->param_sz) {
		push_err(errs, msg_inv_signature, id);
		goto PANIC;
	}

	for (size_t i = 0; i < childsz; ++i) {
		size_t	      chid   = child[i];
		enum jy_ktype expect = ofunc->param_types[i];
		struct kexpr  pexpr  = { 0 };

		if (cmplexpr(asts, tkns, ctx, errs, scope, chid, &pexpr))
			goto PANIC;

		if (expect != pexpr.type) {
			push_err(errs, msg_type_mismatch, id);
			goto PANIC;
		}
	}

	size_t kid;

	if (!find_funck(ctx->pool, ofunc->func, &kid))
		if (add_funck(ctx->pool, ofunc, &kid) != ERROR_SUCCESS)
			goto PANIC;

	write_push(ctx->cnk, kid);
	write_byte(ctx->cnk, JY_OP_CALL);

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
		       size_t		   id,
		       struct kexpr	  *expr)
{
	jry_assert(asts->childsz[id] == 2);

	size_t *child		= asts->child[id];

	enum jy_ktype ktypes[2] = { -1, -1 };

	for (size_t i = 0; i < 2; ++i) {
		struct kexpr pexpr = { 0 };

		if (cmplexpr(asts, tkns, ctx, errs, scope, child[i], &pexpr))
			goto PANIC;

		ktypes[i] = pexpr.type;

		if (ktypes[i] == JY_K_UNKNOWN) {
			push_err(errs, msg_inv_operation, id);
			goto PANIC;
		}
	}

	if (ktypes[0] != ktypes[1]) {
		push_err(errs, msg_inv_operation, id);
		goto PANIC;
	}

	enum jy_ast optype = asts->types[id];
	uint8_t	    code;

	switch (optype) {
	case AST_EQUALITY:
		code	   = JY_OP_CMP;
		expr->type = JY_K_BOOL;
		break;
	case AST_LESSER:
		if (!isnum(ktypes[0])) {
			push_err(errs, msg_inv_operation, id);
			goto PANIC;
		}
		code	   = JY_OP_LT;
		expr->type = JY_K_BOOL;
		break;
	case AST_GREATER:
		if (!isnum(ktypes[0])) {
			push_err(errs, msg_inv_operation, id);
			goto PANIC;
		}
		code	   = JY_OP_GT;
		expr->type = JY_K_BOOL;
		break;
	case AST_ADDITION:
		code	   = JY_OP_ADD;
		expr->type = ktypes[0];
		break;
	case AST_SUBTRACT:
		code	   = JY_OP_SUB;
		expr->type = ktypes[0];
		break;
	case AST_MULTIPLY:
		code	   = JY_OP_MUL;
		expr->type = ktypes[0];
		break;
	case AST_DIVIDE:
		code	   = JY_OP_DIV;
		expr->type = ktypes[0];
		break;
	default:
		push_err(errs, msg_inv_operation, id);
		goto PANIC;
	}

	write_byte(ctx->cnk, code);

	return false;
PANIC:
	return true;
}

static cmplfn_t rules[] = {
	[AST_ROOT]	     = NULL,

	// > statements
	[AST_IMPORT_STMT]    = NULL,
	[AST_INCLUDE_STMT]   = NULL,
	// < statements

	// > declarations
	[AST_RULE_DECL]	     = NULL,
	[AST_INGRESS_DECL]   = NULL,
	[AST_NAME_DECL]	     = NULL,
	// < declarations

	// > sections
	[AST_JUMP_SECT]	     = NULL,
	[AST_INPUT_SECT]     = NULL,
	[AST_MATCH_SECT]     = NULL,
	[AST_CONDITION_SECT] = NULL,
	[AST_FIELD_SECT]     = NULL,
	// < sections

	[AST_LONG_TYPE]	     = NULL,
	[AST_STR_TYPE]	     = NULL,

	// > expression
	[AST_ALIAS]	     = NULL,
	[AST_EVENT]	     = cmplevent,
	[AST_FIELD]	     = cmplfield,
	[AST_CALL]	     = cmplcall,

	[AST_REGMATCH]	     = NULL,
	[AST_EQUALITY]	     = cmplbinary,
	[AST_LESSER]	     = cmplbinary,
	[AST_GREATER]	     = cmplbinary,
	[AST_NOT]	     = NULL,

	[AST_ADDITION]	     = cmplbinary,
	[AST_SUBTRACT]	     = cmplbinary,
	[AST_MULTIPLY]	     = cmplbinary,
	[AST_DIVIDE]	     = cmplbinary,

	[AST_NAME]	     = cmplname,
	[AST_FIELD_NAME]     = NULL,
	[AST_PATH]	     = NULL,

	[AST_REGEXP]	     = cmplliteral,
	[AST_LONG]	     = cmplliteral,
	[AST_STRING]	     = cmplliteral,
	[AST_FALSE]	     = cmplliteral,
	[AST_TRUE]	     = cmplliteral,
	// < expression
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
				 size_t		     id,
				 size_t		   **patchofs,
				 size_t		    *patchsz)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	for (size_t i = 0; i < childsz; ++i) {
		size_t	     chid = child[i];
		struct kexpr expr = { 0 };

		if (cmplexpr(asts, tkns, ctx, errs, ctx->names, chid, &expr))
			continue;

		if (expr.type != JY_K_BOOL) {
			push_err(errs, msg_inv_match_expr, id);
			continue;
		}

		size_t event_id;

		if (!find_ast_type(asts, AST_EVENT, chid, &event_id))
			continue;

		write_byte(ctx->cnk, JY_OP_JMPF);

		jry_mem_push(*patchofs, *patchsz, ctx->cnk->size);

		if (patchofs == NULL)
			goto PANIC;

		*patchsz += 1;

		// Reserved 2 bytes for short jump
		write_byte(ctx->cnk, 0);
		write_byte(ctx->cnk, 0);
	}

	return false;
PANIC:
	return true;
}

static inline bool cmpltargetsect(struct jy_asts     *asts,
				  struct jy_tkns     *tkns,
				  struct jy_scan_ctx *ctx,
				  struct jy_errs     *errs,
				  size_t	      id)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	for (size_t i = 0; i < childsz; ++i) {
		size_t	     chid = child[i];
		struct kexpr expr = { 0 };

		cmplexpr(asts, tkns, ctx, errs, ctx->names, chid, &expr);

		if (expr.type != JY_K_TARGET) {
			push_err(errs, msg_inv_target_expr, id);
			goto PANIC;
		}
	}

	return false;
PANIC:
	return true;
}

static inline bool cmplfieldsect(struct jy_asts	    *asts,
				 struct jy_tkns	    *tkns,
				 struct jy_scan_ctx *ctx,
				 struct jy_errs	    *errs,
				 size_t		     id,
				 const char	    *key,
				 size_t		     keysz)
{
	size_t *child	   = asts->child[id];
	size_t	childsz	   = asts->childsz[id];

	struct jy_defs def = { NULL };

	for (size_t i = 0; i < childsz; ++i) {
		size_t op_id   = child[i];
		size_t name_id = asts->child[op_id][0];
		size_t type_id = asts->child[op_id][1];
		char  *name    = tkns->lexemes[asts->tkns[name_id]];
		size_t length  = tkns->lexsz[asts->tkns[name_id]];

		// Todo: redeclaration error
		if (jry_find_def(&def, name, length, NULL)) {
			push_err(errs, msg_redefinition, id);
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
		default:
			push_err(errs, msg_inv_type, id);
			goto PANIC;
		}

		int e = jry_add_def(&def, name, length, (jy_val_t) NULL, ktype);

		if (e != ERROR_SUCCESS)
			goto PANIC;
	}

	size_t oldsz	  = ctx->events->size;
	size_t allocsz	  = sizeof(def) * (oldsz + 1);
	ctx->events->defs = jry_realloc(ctx->events->defs, allocsz);

	if (ctx->events->defs == NULL)
		goto PANIC;

	ctx->events->defs[oldsz] = def;
	jy_val_t val		 = jry_def2v(&ctx->events->defs[oldsz]);

	if (jry_add_def(ctx->names, key, keysz, val, JY_K_EVENT) != 0)
		goto PANIC;

	ctx->events->size += 1;

	return false;
PANIC:
	jry_free_def(def);
	return true;
}

static inline bool cmplruledecl(struct jy_asts	   *asts,
				struct jy_tkns	   *tkns,
				struct jy_scan_ctx *ctx,
				struct jy_errs	   *errs,
				size_t		    id)
{
	bool panic	 = false;

	size_t *child	 = asts->child[id];
	size_t	childsz	 = asts->childsz[id];

	// Short jump patch offset
	// this will jump to the end of the current rule
	size_t *patchofs = NULL;
	size_t	patchsz	 = 0;

	size_t matchs[childsz];
	size_t targets[childsz];
	size_t conds[childsz];

	size_t matchsz	= 0;
	size_t targetsz = 0;
	size_t condsz	= 0;

	for (size_t i = 0; i < childsz; ++i) {
		size_t	    chid = child[i];
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
		default:
			push_err(errs, msg_inv_rule_sect, id);
			continue;
		}
	}

	if (matchsz == 0) {
		push_err(errs, msg_missing_match_sect, id);
		goto PANIC;
	}

	if (targetsz == 0) {
		push_err(errs, msg_missing_target_sect, id);
		goto PANIC;
	}

	for (size_t i = 0; i < matchsz; ++i) {
		size_t id = matchs[i];
		cmplmatchsect(asts, tkns, ctx, errs, id, &patchofs, &patchsz);
	}

	for (size_t i = 0; i < targetsz; ++i) {
		size_t id = targets[i];
		cmpltargetsect(asts, tkns, ctx, errs, id);
	}

	size_t endofs = ctx->cnk->size;

	for (size_t i = 0; i < patchsz; ++i) {
		size_t ofs = patchofs[i];
		short  jmp = (short) (endofs - ofs + 1);

		memcpy(ctx->cnk->codes + ofs, &jmp, sizeof(jmp));
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
				   size_t	       id)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	char  *lex	= tkns->lexemes[asts->tkns[id]];
	size_t lexsz	= tkns->lexsz[asts->tkns[id]];

	size_t fields[childsz];
	size_t fieldsz = 0;

	// Todo: redeclaration error
	if (jry_find_def(ctx->names, lex, lexsz, NULL))
		goto PANIC;

	for (size_t i = 0; i < childsz; ++i) {
		size_t	    chid = child[i];
		enum jy_ast type = asts->types[chid];

		switch (type) {
		case AST_FIELD_SECT:
			fields[fieldsz++] = chid;
			break;
		default:
			break;
		}
	}

	for (size_t i = 0; i < fieldsz; ++i)
		cmplfieldsect(asts, tkns, ctx, errs, fields[i], lex, lexsz);

	return false;

PANIC:
	return true;
}

static inline bool cmplimportstmt(struct jy_asts     *asts,
				  struct jy_tkns     *tkns,
				  struct jy_scan_ctx *ctx,
				  struct jy_errs     *errs,
				  size_t	      id)
{
	size_t tkn	= asts->tkns[id];
	char  *lexeme	= tkns->lexemes[tkn];
	size_t lexsz	= tkns->lexsz[tkn];
	int    dirlen	= strlen(ctx->modules->dir);
	char   prefix[] = "lib";

	char path[dirlen + sizeof(prefix) - 1 + lexsz];

	strcpy(path, ctx->modules->dir);
	strcat(path, prefix);
	strncat(path, lexeme, lexsz);

	int module = jry_module_load(path);

	if (module < 0) {
		const char *msg = jry_module_error(module);
		push_err(errs, msg, id);
		return true;
	}

	struct jy_defs *def = jry_module_def(module);

	jry_mem_push(ctx->modules->list, ctx->modules->size, module);
	ctx->modules->size += 1;

	jy_val_t v	    = jry_def2v(def);

	if (jry_add_def(ctx->names, lexeme, lexsz, v, JY_K_MODULE) != 0)
		return true;

	return false;
}

static inline bool cmplroot(struct jy_asts     *asts,
			    struct jy_tkns     *tkns,
			    struct jy_scan_ctx *ctx,
			    struct jy_errs     *errs,
			    size_t		id)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	size_t ingress[childsz];
	size_t rules[childsz];
	size_t imports[childsz];

	size_t imports_sz = 0;
	size_t ingress_sz = 0;
	size_t rules_sz	  = 0;

	for (size_t i = 0; i < childsz; ++i) {
		size_t	    chid = child[i];
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

	for (size_t i = 0; i < imports_sz; ++i)
		cmplimportstmt(asts, tkns, ctx, errs, imports[i]);

	for (size_t i = 0; i < ingress_sz; ++i)
		cmplingressdecl(asts, tkns, ctx, errs, ingress[i]);

	for (size_t i = 0; i < rules_sz; ++i)
		cmplruledecl(asts, tkns, ctx, errs, rules[i]);

	write_byte(ctx->cnk, JY_OP_END);

	return false;
}

void jry_compile(struct jy_asts	    *asts,
		 struct jy_tkns	    *tkns,
		 struct jy_scan_ctx *ctx,
		 struct jy_errs	    *errs)
{
	size_t	    root = 0;
	enum jy_ast type = asts->types[root];

	if (type != AST_ROOT)
		return;

	cmplroot(asts, tkns, ctx, errs, root);
}

void jry_free_scan_ctx(struct jy_scan_ctx ctx)
{
	for (size_t i = 0; i < ctx.modules->size; ++i)
		jry_module_unload(ctx.modules->list[i]);

	jry_free(ctx.modules->list);
	jry_free(ctx.cnk->codes);

	jry_free(ctx.pool->vals);
	jry_free(ctx.pool->types);
	jry_free(ctx.pool->obj);

	for (size_t i = 0; i < ctx.events->size; ++i)
		jry_free_def(ctx.events->defs[i]);

	jry_free(ctx.events->defs);

	if (ctx.names)
		jry_free_def(*ctx.names);
}
