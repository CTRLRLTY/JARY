#include "compiler.h"

#include "ast.h"
#include "token.h"

#include "jary/error.h"
#include "jary/fnv.h"
#include "jary/memory.h"
#include "jary/value.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct kexpr {
	size_t	      id;
	enum jy_ktype type;
	/* data */
};

// expression compile subroutine signature
typedef bool (*cmplfn_t)(struct jy_asts *, struct jy_tkns *,
			 struct jy_scan_ctx *, struct jy_cerrs *, size_t,
			 struct kexpr *);

inline static cmplfn_t rule(enum jy_ast type);

static bool isnum(enum jy_ktype type)
{
	switch (type) {
	case JY_K_LONG:
		return true;
	}

	return false;
}

inline static void addname(struct jy_names *names, const char *str,
			   size_t length, enum jy_ktype **type)
{
	strhash_t hash = jry_strhash(str, length);
	char	 *name = strndup(str, length);

	jry_mem_push(names->hashs, names->size, hash);
	jry_mem_push(names->strs, names->size, name);
	jry_mem_push(names->strsz, names->size, length);
	jry_mem_push(names->types, names->size, *type);

	names->size += 1;
}

inline static void addbyte(struct jy_chunks *cnk, uint8_t code)
{
	jry_mem_push(cnk->codes, cnk->size, code);

	cnk->size += 1;
}

static void addpush(struct jy_chunks *cnk, size_t constant)
{
	uint8_t width = 0;

	union {
		size_t num;
		char   c[sizeof(constant)];
	} v;

	v.num = constant;

	if (constant <= 0xff) {
		addbyte(cnk, JY_OP_PUSH8);
		width = 1;
	} else if (constant <= 0xffff) {
		addbyte(cnk, JY_OP_PUSH16);
		width = 2;
	} else if (constant <= 0xffffffff) {
		addbyte(cnk, JY_OP_PUSH32);
		width = 4;
	} else {
		addbyte(cnk, JY_OP_PUSH64);
		width = 8;
	}

	for (uint8_t i = 0; i < width; ++i)
		addbyte(cnk, v.c[i]);
}

inline static bool namexist(struct jy_names *name, const char *str,
			    size_t length, size_t *id)
{
	strhash_t hash = jry_strhash(str, length);

	size_t i;

	for (i = 0; i < name->size; ++i) {
		char	  a   = name->strs[i][0];
		strhash_t tmp = name->hashs[i];
		size_t	  len = name->strsz[i];

		if (a == str[0] && tmp == hash && len == length)
			goto FOUND;
	}

	return false;

FOUND:
	if (id != NULL)
		*id = i;
	return true;
}

static bool val_equal(jy_val_t a, jy_val_t b, enum jy_ktype a_type,
		      enum jy_ktype b_type)
{
	if (a_type != b_type)
		return false;

	switch (a_type) {
	case JY_K_LONG: {
		int64_t av = jry_val_long(a);
		int64_t bv = jry_val_long(b);

		return av == bv;
		break;
	}

	case JY_K_STR: {
		struct jy_obj_str *av = jry_val_str(a);
		struct jy_obj_str *bv = jry_val_str(b);

		return av->str[0] == bv->str[0] && av->hash == bv->hash;
	}
	}

	return false;
}

static bool findk(struct jy_kpool *pool, jy_val_t val, enum jy_ktype type,
		  size_t *id)
{
	for (size_t i = 0; i < pool->size; ++i) {
		enum jy_ktype t = pool->types[i];
		jy_val_t      v = pool->vals[i];

		if (val_equal(v, val, t, type)) {
			if (id != NULL)
				*id = i;
			return true;
		}
	}

	return false;
}

static size_t addk(struct jy_kpool *pool, jy_val_t val, enum jy_ktype type)
{
	jry_mem_push(pool->vals, pool->size, val);
	jry_mem_push(pool->types, pool->size, type);

	return pool->size++;
}

inline static bool cmplexpr(struct jy_asts *asts, struct jy_tkns *tkns,
			    struct jy_scan_ctx *ctx, struct jy_cerrs *errs,
			    size_t id, struct kexpr *expr)
{
	enum jy_ast type = asts->types[id];
	cmplfn_t    fn	 = rule(type);

	return fn(asts, tkns, ctx, errs, id, expr);
}

static bool cmplliteral(struct jy_asts *asts, struct jy_tkns *tkns,
			struct jy_scan_ctx *ctx, struct jy_cerrs *errs,
			size_t id, struct kexpr *expr)
{
	enum jy_ast type    = asts->types[id];

	jy_val_t      val   = 0;
	enum jy_ktype vtype = -1;

	size_t tkn	    = asts->tkns[id];
	char  *lexeme	    = tkns->lexemes[tkn];
	size_t lexsz	    = tkns->lexsz[tkn];

	switch (type) {
	case AST_LONG: {
		long num = strtol(lexeme, NULL, 10);
		val	 = jry_long_val(num);
		vtype	 = JY_K_LONG;

		break;
	}
	case AST_STRING: {
		char *str = strndup(lexeme + 1, lexsz - 1);
		// -2 to not include ""
		val	  = jry_str_val(str, lexsz - 2);
		vtype	  = JY_K_STR;

		break;
	}
	default:
		goto PANIC;
	}

	if (!findk(ctx->pool, val, vtype, &expr->id))
		expr->id = addk(ctx->pool, val, vtype);

	addpush(ctx->cnk, expr->id);

	return false;

PANIC:
	return true;
}

static bool cmplcall(struct jy_asts *asts, struct jy_tkns *tkns,
		     struct jy_scan_ctx *ctx, struct jy_cerrs *errs, size_t id,
		     struct kexpr *expr)
{
	size_t *child	= asts->child[id];
	size_t	childsz = asts->childsz[id];

	size_t tkn	= asts->tkns[id];
	char  *lexeme	= tkns->lexemes[tkn];
	size_t lexsz	= tkns->lexsz[tkn];

	size_t fnid;

	if (!namexist(ctx->names, lexeme, lexsz, &fnid))
		goto PANIC;

	enum jy_ktype	  *type = ctx->names->types[fnid];
	struct jy_funcdef *def	= (struct jy_funcdef *) type;

	if (def->param_sz != childsz)
		goto PANIC;

	for (size_t i = 0; i < childsz; ++i) {
		size_t chid	   = child[i];

		struct kexpr pexpr = { 0 };

		if (cmplexpr(asts, tkns, ctx, errs, chid, &pexpr))
			goto PANIC;

		enum jy_ktype expect = def->param_types[i];

		if (pexpr.type != expect)
			goto PANIC;
	}

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
		goto PANIC;
	}

	addbyte(ctx->cnk, code);

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

	// > expression
	[AST_ALIAS]	= NULL,
	[AST_EVENT]	= NULL,
	[AST_MEMBER]	= NULL,
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

	[AST_NAME]	= NULL,
	[AST_PATH]	= NULL,

	[AST_REGEXP]	= cmplliteral,
	[AST_LONG]	= cmplliteral,
	[AST_STRING]	= cmplliteral,
	[AST_FALSE]	= cmplliteral,
	[AST_TRUE]	= cmplliteral,
	// < expression
};

inline static cmplfn_t rule(enum jy_ast type)
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
	}

PANIC:
	return true;
}

static bool cmplingress(struct jy_asts *asts, struct jy_tkns *tkns,
			struct jy_scan_ctx *ctx, struct jy_cerrs *errs,
			size_t id)
{
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
		size_t chid	   = child[i];

		enum jy_ast   decl = asts->types[chid];
		enum jy_ktype ktype;

		switch (decl) {
		case AST_RULE:
			ktype		     = JY_K_RULE;
			chrules[chrules_sz]  = chid;
			chrules_sz	    += 1;
			break;
		case AST_INGRESS:
			ktype			 = JY_K_INGRESS;
			chingress[chingress_sz]	 = chid;
			chingress_sz		+= 1;
			break;
		case AST_IMPORT:
			chimport[chimport_sz]  = chid;
			chimport_sz	      += 1;
			break;
		}
	}

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

void jry_define_func(struct jy_names *names, const char *name, size_t length,
		     enum jy_ktype returntype, enum jy_ktype *paramtypes,
		     size_t paramsz, void *cfunc)
{
	struct jy_funcdef *func = jry_alloc(sizeof(*func));

	func->type		= JY_K_FUNC;
	func->return_type	= returntype;
	func->param_types	= paramtypes;
	func->param_sz		= paramsz;
	func->func		= cfunc;

	addname(names, name, length, (enum jy_ktype **) &func->type);
}

void jry_compile(struct jy_asts *asts, struct jy_tkns *tkns,
		 struct jy_scan_ctx *ctx, struct jy_cerrs *errs)
{
	size_t	    root = 0;
	enum jy_ast type = asts->types[root];

	jry_assert(type == AST_ROOT);

	cmplroot(asts, tkns, ctx, errs, root);
}

void jry_free_scan_ctx(struct jy_scan_ctx *ctx)
{
	jry_free(ctx->pool->vals);
	jry_free(ctx->pool->types);

	jry_free(ctx->names->hashs);

	for (size_t i = 0; i < ctx->names->size; ++i) {
		jry_free(ctx->names->strs[i]);
		jry_free(ctx->names->types[i]);
	}

	jry_free(ctx->names->strs);
	jry_free(ctx->names->strsz);
	jry_free(ctx->names->types);

	jry_free(ctx->cnk->codes);
}