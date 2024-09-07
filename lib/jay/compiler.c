#include "compiler.h"

#include "ast.h"
#include "token.h"

#include "jary/common.h"
#include "jary/error.h"
#include "jary/memory.h"
#include "jary/object.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct kexpr {
	size_t	      id;
	enum jy_ktype type;
};

// expression compile subroutine signature
typedef bool (*cmplfn_t)(struct jy_asts *, struct jy_tkns *,
			 struct jy_scan_ctx *, struct jy_cerrs *, size_t,
			 struct kexpr *);

static inline cmplfn_t rule(enum jy_ast type);

static bool isnum(enum jy_ktype type)
{
	switch (type) {
	case JY_K_LONG:
		return true;
	default:
		return false;
	}
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

static inline bool find_strk(struct jy_kpool *pool, const char *str,
			     size_t length, size_t *id)
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

static inline bool find_funck(struct jy_kpool *pool, jy_funcptr_t fn,
			      size_t *id)
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

USE_RESULT
static inline int add_longk(struct jy_kpool *pool, long num, size_t *id)
{
	jy_val_t val = jry_long2v(num);

	jry_mem_push(pool->vals, pool->size, val);
	jry_mem_push(pool->types, pool->size, JY_K_LONG);

	if (pool->vals == NULL || pool->types == NULL)
		return ERROR_NOMEM;

	*id = pool->size++;

	return ERROR_SUCCESS;
}

USE_RESULT
static inline int add_strk(struct jy_kpool *pool, const char *str,
			   size_t length, size_t *id)
{
	uint32_t moffs1	 = sizeof(struct jy_obj_str);
	uint32_t moffs2	 = moffs1 + sizeof(length);

	size_t objsz	 = pool->objsz;

	pool->objsz	+= moffs2;
	pool->obj	 = jry_realloc(pool->obj, pool->objsz);

	if (pool->obj == NULL)
		return ERROR_NOMEM;

	char *mem		= ((char *) pool->obj) + objsz;

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

USE_RESULT
static inline int add_funck(struct jy_kpool *pool, struct jy_obj_func *func,
			    size_t *id)
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

static inline bool cmplexpr(struct jy_asts *asts, struct jy_tkns *tkns,
			    struct jy_scan_ctx *ctx, struct jy_cerrs *errs,
			    size_t id, struct kexpr *expr)
{
	enum jy_ast type = asts->types[id];
	cmplfn_t    fn	 = rule(type);

	return fn(asts, tkns, ctx, errs, id, expr);
}

static bool cmplliteral(struct jy_asts *asts, struct jy_tkns *tkns,
			struct jy_scan_ctx *ctx, struct jy_cerrs *UNUSED(errs),
			size_t id, struct kexpr *expr)
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
		goto PANIC;
	}

	write_push(ctx->cnk, expr->id);

	return false;

PANIC:
	return true;
}

static bool cmplname(struct jy_asts *asts, struct jy_tkns *tkns,
		     struct jy_scan_ctx *ctx, struct jy_cerrs *errs, size_t id,
		     struct kexpr *expr)
{
	size_t tkn    = asts->tkns[id];
	char  *lexeme = tkns->lexemes[tkn];
	size_t lexsz  = tkns->lexsz[tkn];

	size_t nid;
	if (!jry_find_def(ctx->names, lexeme, lexsz, &nid))
		goto PANIC;

	struct jy_defs	  *def	  = jry_v2def(ctx->names->vals[nid]);
	struct jy_scan_ctx newctx = *ctx;
	newctx.names		  = def;

	size_t *child		  = asts->child[id];
	size_t	childsz		  = asts->childsz[id];

	for (size_t i = 0; i < childsz; ++i) {
		size_t chid = child[i];

		if (cmplexpr(asts, tkns, &newctx, errs, chid, expr))
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static bool cmplcall(struct jy_asts *asts, struct jy_tkns *tkns,
		     struct jy_scan_ctx *ctx, struct jy_cerrs *errs, size_t id,
		     struct kexpr *expr)
{
	size_t tkn    = asts->tkns[id];
	char  *lexeme = tkns->lexemes[tkn];
	size_t lexsz  = tkns->lexsz[tkn];

	size_t nid;

	if (!jry_find_def(ctx->names, lexeme, lexsz, &nid))
		goto PANIC;

	enum jy_ktype type = ctx->names->types[nid];

	if (type != JY_K_FUNC)
		goto PANIC;

	jy_val_t	    val	  = ctx->names->vals[nid];
	struct jy_obj_func *ofunc = jry_v2func(val);

	size_t *child		  = asts->child[id];
	size_t	childsz		  = asts->childsz[id];

	if (childsz != ofunc->param_sz)
		goto PANIC;

	for (size_t i = 0; i < childsz; ++i) {
		size_t	      chid   = child[i];
		enum jy_ktype expect = ofunc->param_types[i];
		struct kexpr  pexpr  = { 0 };

		if (cmplexpr(asts, tkns, ctx, errs, chid, &pexpr))
			goto PANIC;

		if (expect != pexpr.type)
			goto PANIC;
	}

	size_t kid;

	if (find_funck(ctx->pool, ofunc->func, &kid))
		if (add_funck(ctx->pool, ofunc, &kid) != ERROR_SUCCESS)
			goto PANIC;

	write_push(ctx->cnk, kid);
	write_byte(ctx->cnk, JY_OP_CALL);

	return false;
PANIC:
	return true;
}

static bool cmplbinary(struct jy_asts *asts, struct jy_tkns *tkns,
		       struct jy_scan_ctx *ctx, struct jy_cerrs *errs,
		       size_t id, struct kexpr *expr)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	jry_assert(childsz == 2);

	enum jy_ktype ktypes[2] = { -1, -1 };

	for (size_t i = 0; i < 2; ++i) {
		struct kexpr pexpr = { 0 };

		if (cmplexpr(asts, tkns, ctx, errs, child[i], &pexpr))
			goto PANIC;

		ktypes[i] = pexpr.type;

		if (ktypes[i] == JY_K_UNKNOWN)
			goto PANIC;
	}

	if (ktypes[0] != ktypes[1])
		goto PANIC;

	enum jy_ast optype = asts->types[id];
	uint8_t	    code;

	switch (optype) {
	case AST_EQUALITY:
		code	   = JY_OP_CMP;
		expr->type = JY_K_BOOL;
		break;

	case AST_LESSER:
		if (!isnum(ktypes[0]))
			goto PANIC;
		code	   = JY_OP_LT;
		expr->type = JY_K_BOOL;
		break;
	case AST_GREATER:
		if (!isnum(ktypes[0]))
			goto PANIC;
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
		goto PANIC;
	}

	write_byte(ctx->cnk, code);

	return false;
PANIC:
	return true;
}

static cmplfn_t rules[] = {
	[AST_ROOT]	= NULL,

	// > declarations
	[AST_RULE]	= NULL,
	[AST_IMPORT]	= NULL,
	[AST_INCLUDE]	= NULL,
	[AST_INGRESS]	= NULL,
	// < declarations

	// > sections
	[AST_JUMP]	= NULL,
	[AST_INPUT]	= NULL,
	[AST_MATCH]	= NULL,
	[AST_CONDITION] = NULL,
	[AST_FIELDS]	= NULL,
	// < sections

	[AST_SET_TYPE]	= NULL,
	[AST_LONG_TYPE] = NULL,
	[AST_STR_TYPE]	= NULL,
	[AST_FIELD]	= NULL,

	// > expression
	[AST_ALIAS]	= NULL,
	[AST_EVENT]	= NULL,
	[AST_CALL]	= cmplcall,

	[AST_REGMATCH]	= NULL,
	[AST_EQUALITY]	= cmplbinary,
	[AST_LESSER]	= cmplbinary,
	[AST_GREATER]	= cmplbinary,
	[AST_NOT]	= NULL,

	[AST_ADDITION]	= cmplbinary,
	[AST_SUBTRACT]	= cmplbinary,
	[AST_MULTIPLY]	= cmplbinary,
	[AST_DIVIDE]	= cmplbinary,

	[AST_NAME]	= cmplname,
	[AST_PATH]	= NULL,

	[AST_REGEXP]	= cmplliteral,
	[AST_LONG]	= cmplliteral,
	[AST_STRING]	= cmplliteral,
	[AST_FALSE]	= cmplliteral,
	[AST_TRUE]	= cmplliteral,
	// < expression
};

static inline cmplfn_t rule(enum jy_ast type)
{
	jry_assert(rules[type] != NULL);

	return rules[type];
}

static bool cmplmatch(struct jy_asts *asts, struct jy_tkns *tkns,
		      struct jy_scan_ctx *ctx, struct jy_cerrs *errs, size_t id)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	for (size_t i = 0; i < childsz; ++i) {
		size_t	     chid = child[i];
		struct kexpr expr = { 0 };

		// Todo: handle panic
		if (cmplexpr(asts, tkns, ctx, errs, chid, &expr))
			goto PANIC;

		if (expr.type != JY_K_BOOL)
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static bool cmplfield(struct jy_asts *asts, struct jy_tkns *tkns,
		      struct jy_scan_ctx *ctx, struct jy_cerrs *errs, size_t id,
		      struct jy_defs *def)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	for (size_t i = 0; i < childsz; ++i) {
		size_t op_id   = child[i];
		size_t name_id = asts->child[op_id][0];
		size_t type_id = asts->child[op_id][1];

		char  *name    = tkns->lexemes[asts->tkns[name_id]];
		size_t length  = tkns->lexsz[asts->tkns[name_id]];

		// Todo: redeclaration error
		if (jry_find_def(def, name, length, NULL))
			goto PANIC;

		enum jy_ast type = asts->types[type_id];

		int e = jry_add_def(def, name, length, (jy_val_t) NULL, type);

		if (e != ERROR_SUCCESS)
			goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static bool cmplrule(struct jy_asts *asts, struct jy_tkns *tkns,
		     struct jy_scan_ctx *ctx, struct jy_cerrs *errs, size_t id)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	for (size_t i = 0; i < childsz; ++i) {
		size_t	    chid = child[i];
		enum jy_ast type = asts->types[chid];

		bool panic	 = true;

		switch (type) {
		case AST_MATCH:
			panic = cmplmatch(asts, tkns, ctx, errs, chid);
		default:
			// Todo: handle mismatch section
			goto PANIC;
		}

		if (panic)
			goto PANIC;
	}

PANIC:
	return true;
}

static inline bool cmplingress(struct jy_asts *asts, struct jy_tkns *tkns,
			       struct jy_scan_ctx *ctx, struct jy_cerrs *errs,
			       size_t id)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	for (size_t i = 0; i < childsz; ++i) {
		size_t	    chid = child[i];
		enum jy_ast type = asts->types[chid];

		bool panic	 = true;

		switch (type) {
		case AST_FIELDS: {
			struct jy_defs def = { NULL };

			panic = cmplfield(asts, tkns, ctx, errs, chid, &def);
			jry_mem_push(ctx->events->defs, ctx->events->size, def);
			ctx->events->size += 1;
		}
		default:
			// Todo: handle mismatch section
			goto PANIC;
		}

		if (panic)
			goto PANIC;
	}

	return false;

PANIC:
	return true;
}

static bool cmplimport(struct jy_asts *asts, struct jy_tkns *tkns,
		       struct jy_scan_ctx *ctx, struct jy_cerrs *UNUSED(errs),
		       size_t id)
{
	size_t tkn    = asts->tkns[id];
	char  *lexeme = tkns->lexemes[tkn];
	size_t lexsz  = tkns->lexsz[tkn];

	int dirlen    = strlen(ctx->modules->dir);

	char prefix[] = "lib";

	char path[dirlen + sizeof(prefix) - 1 + lexsz];

	strcpy(path, ctx->modules->dir);
	strcat(path, prefix);
	strncat(path, lexeme, lexsz);

	struct jy_defs	     *def  = NULL;
	struct jy_module_ctx *mctx = NULL;

	int status		   = jry_module_load(path, &def, &mctx);

	// Todo: handle unable to load module
	if (status != ERROR_SUCCESS)
		goto PANIC;

	jry_mem_push(ctx->modules->list, ctx->modules->size, mctx);
	ctx->modules->size += 1;

	status = jry_add_def(ctx->names, lexeme, lexsz, jry_def2v(def),
			     JY_K_MODULE);

	if (status != ERROR_SUCCESS)
		goto PANIC;

	return false;
PANIC:
	return true;
}

static bool cmplroot(struct jy_asts *asts, struct jy_tkns *tkns,
		     struct jy_scan_ctx *ctx, struct jy_cerrs *errs, size_t id)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	size_t chingress[childsz];
	size_t chingress_sz = 0;

	size_t chimport[childsz];
	size_t chimport_sz = 0;

	size_t chrules[childsz];
	size_t chrules_sz = 0;

	for (size_t i = 0; i < childsz; ++i) {
		size_t	    chid = child[i];
		enum jy_ast decl = asts->types[chid];

		switch (decl) {
		case AST_RULE:
			chrules[chrules_sz]  = chid;
			chrules_sz	    += 1;
			break;
		case AST_INGRESS:
			chingress[chingress_sz]	 = chid;
			chingress_sz		+= 1;
			break;
		case AST_IMPORT:
			chimport[chimport_sz]  = chid;
			chimport_sz	      += 1;
			break;
		default:
			break;
		}
	}

	for (size_t i = 0; i < chimport_sz; ++i)
		if (cmplimport(asts, tkns, ctx, errs, chimport[i]))
			goto PANIC;

	for (size_t i = 0; i < chingress_sz; ++i)
		if (cmplingress(asts, tkns, ctx, errs, chingress[i]))
			goto PANIC;

	for (size_t i = 0; i < chrules_sz; ++i)
		if (cmplrule(asts, tkns, ctx, errs, chrules[i]))
			goto PANIC;

	return false;

PANIC:
	return true;
}

void jry_compile(struct jy_asts *asts, struct jy_tkns *tkns,
		 struct jy_scan_ctx *ctx, struct jy_cerrs *errs)
{
	size_t	    root = 0;
	enum jy_ast type = asts->types[root];

	if (type != AST_ROOT)
		return;

	cmplroot(asts, tkns, ctx, errs, root);
}

void jry_free_scan_ctx(struct jy_scan_ctx *ctx)
{
	for (size_t i = 0; i < ctx->modules->size; ++i)
		jry_module_unload(ctx->modules->list[i]);

	jry_free(ctx->modules->list);
	jry_free(ctx->cnk->codes);

	jry_free(ctx->pool->vals);
	jry_free(ctx->pool->types);
	jry_free(ctx->pool->obj);

	for (size_t i = 0; i < ctx->events->size; ++i)
		jry_free_def(&ctx->events->defs[i]);

	jry_free(ctx->events->defs);

	jry_free_def(ctx->names);
}