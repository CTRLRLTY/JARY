
#include "parser.h"

#include "scanner.h"

#include "jary/error.h"
#include "jary/memory.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(__a, __b) ((__b > __a) ? __b : __a)

struct parser {
	const char *src;
	size_t	    srcsz;

	// current tkn id
	size_t tkn;
};

enum prec {
	PREC_NONE,
	PREC_ASSIGNMENT,
	PREC_OR,	 // or
	PREC_AND,	 // and
	PREC_EQUALITY,	 // == !=
	PREC_COMPARISON, // < > <= >=
	PREC_TERM,	 // + -
	PREC_FACTOR,	 // * /
	PREC_UNARY,	 // ! -
	PREC_CALL,	 // . () ~
	PREC_LAST,
};

typedef bool (*parsefn_t)(struct parser *, struct jy_asts *, struct jy_tkns *,
			  struct jy_prserrs *, enum jy_ast, size_t *);

struct rule {
	parsefn_t prefix;
	parsefn_t infix;
	enum prec prec;
};

static struct rule *rule(enum jy_tkn type);

static bool _precedence(struct parser *p, struct jy_asts *asts,
			struct jy_tkns *tkns, struct jy_prserrs *errs,
			enum jy_ast sect, size_t *topast, enum prec rbp);

static bool _expression(struct parser *p, struct jy_asts *asts,
			struct jy_tkns *tkns, struct jy_prserrs *errs,
			enum jy_ast sect, size_t *topast);

inline static void adderr(struct jy_prserrs *errs, size_t line, size_t ofs,
			  const char *lexeme, size_t lexsz, const char *msg,
			  size_t msgsz)
{
	jry_mem_push(errs->lines, errs->size, line);
	jry_mem_push(errs->ofs, errs->size, ofs);
	jry_mem_push(errs->lexemes, errs->size, strndup(lexeme, lexsz));
	jry_mem_push(errs->msgs, errs->size, strndup(msg, msgsz));

	errs->size += 1;
}

inline static void errtkn(struct jy_prserrs *errs, struct jy_tkns *tkns,
			  size_t tknid, const char *msg, size_t msgsz)
{
	jry_assert(tkns->size > tknid);

	size_t line = tkns->lines[tknid];
	size_t ofs  = tkns->ofs[tknid];
	char  *lex  = tkns->lexemes[tknid];
	size_t len  = tkns->lexsz[tknid];

	adderr(errs, line, ofs, lex, len, msg, msgsz);
}

inline static size_t addast(struct jy_asts *asts, enum jy_ast type, size_t tkn,
			    size_t *child, size_t childsz)
{
	jry_mem_push(asts->types, asts->size, type);
	jry_mem_push(asts->tkns, asts->size, tkn);
	jry_mem_push(asts->child, asts->size, child);
	jry_mem_push(asts->childsz, asts->size, childsz);

	return asts->size++;
}

inline static void popast(struct jy_asts *asts)
{
	jry_assert(asts->size > 0);

	size_t	 id    = asts->size - 1;
	size_t **child = &asts->child[id];

	jry_free(*child);

	*child	    = NULL;

	asts->size -= 1;
}

inline static size_t addchild(struct jy_asts *asts, size_t astid,
			      size_t childid)
{
	jry_assert(asts->size > astid);
	jry_assert(asts->size > childid);

	size_t **child	 = &asts->child[astid];
	size_t	*childsz = &asts->childsz[astid];

	if (*childsz == 0) {
		*child = jry_alloc(sizeof(**child));
	} else {
		size_t degree = *childsz + 1;
		*child	      = jry_realloc(*child, sizeof(**child) * degree);
	}

	(*child)[*childsz] = childid;

	return (*childsz)++;
}

inline static size_t addtkn(struct jy_tkns *tkns, enum jy_tkn type, size_t line,
			    size_t ofs, char *lexeme, size_t lexsz)
{
	jry_mem_push(tkns->types, tkns->size, type);
	jry_mem_push(tkns->lines, tkns->size, line);
	jry_mem_push(tkns->ofs, tkns->size, ofs);
	jry_mem_push(tkns->lexemes, tkns->size, lexeme);
	jry_mem_push(tkns->lexsz, tkns->size, lexsz);

	return tkns->size++;
}

// return last non newline or eof tkn
inline static size_t find_last_tkn(struct jy_tkns *tkns)
{
	jry_assert(tkns->size > 0);

	size_t	    tkn = tkns->size - 1;
	enum jy_tkn t	= tkns->types[tkn];

	while (tkn < tkns->size && (t == TKN_NEWLINE || t == TKN_EOF)) {
		tkn--;
		t = tkns->types[tkn];
	}

	return tkn;
}

// check if section tkn
inline static bool issect(enum jy_tkn type)
{
	switch (type) {
	case TKN_INPUT:
	case TKN_MATCH:
	case TKN_TARGET:
	case TKN_CONDITION:
	case TKN_FIELDS:
		return true;
	}

	return false;
}

// check if declaration token
inline static bool isdecl(enum jy_tkn type)
{
	switch (type) {
	case TKN_RULE:
	case TKN_IMPORT:
	case TKN_INGRESS:
	case TKN_INCLUDE:
		return true;
	}

	return false;
}

inline static bool ended(const enum jy_tkn *types, size_t length)
{
	return types[length - 1] == TKN_EOF;
}

// advance scanner and fill tkn
inline static void advance(struct jy_tkns *tkns, const char **src,
			   size_t *srcsz)
{
	enum jy_tkn type;

	// getting previous line
	size_t line  = tkns->lines[tkns->size - 1];
	// getting previous ofs
	size_t ofs   = tkns->ofs[tkns->size - 1];
	size_t lexsz = 0;
	char  *lex   = NULL;

	size_t read  = jry_scan(*src, *srcsz, &type, &line, &ofs, &lex, &lexsz);

	*src   += read;
	*srcsz	= (*srcsz > read) ? *srcsz - read : 0;

	addtkn(tkns, type, line, ofs, lex, lexsz);
}

inline static void next(struct jy_tkns *tkns, const char **src, size_t *srcsz,
			size_t *tkn)
{
	advance(tkns, src, srcsz);
	*tkn = tkns->size - 1;
}

inline static void skipnewline(struct jy_tkns *tkns, const char **src,
			       size_t *srcsz, size_t *tkn)
{
	while (tkns->types[*tkn] == TKN_NEWLINE)
		next(tkns, src, srcsz, tkn);
}

inline static bool synclist(struct jy_tkns *tkns, const char **src,
			    size_t *srcsz, size_t *tkn)
{
	enum jy_tkn t = tkns->types[*tkn];

	while (!isdecl(t) && !issect(t)) {
		if (t == TKN_RIGHT_BRACE || t == TKN_NEWLINE)
			goto SYNCED;

		if (t == TKN_EOF)
			goto PANIC;

		next(tkns, src, srcsz, tkn);
		t = tkns->types[*tkn];
	}

SYNCED:
	return false;
PANIC:
	return true;
}

inline static bool syncsection(struct jy_tkns *tkns, const char **src,
			       size_t *srcsz, size_t *tkn)
{
	enum jy_tkn t = tkns->types[*tkn];

	while (!isdecl(t) && !issect(t)) {
		if (t == TKN_RIGHT_BRACE)
			goto SYNCED;

		if (t == TKN_EOF)
			goto PANIC;

		next(tkns, src, srcsz, tkn);
		t = tkns->types[*tkn];
	}

SYNCED:
	return false;
PANIC:
	return true;
}

static bool syncdecl(struct jy_tkns *tkns, const char **src, size_t *srcsz,
		     size_t *tkn)
{
	enum jy_tkn t = tkns->types[*tkn];

	while (!isdecl(t)) {
		if (t == TKN_EOF)
			goto PANIC;

		next(tkns, src, srcsz, tkn);
		t = tkns->types[*tkn];
	}

	return false;
PANIC:
	return true;
}

static bool _err(struct parser *p, struct jy_asts *asts, struct jy_tkns *tkns,
		 struct jy_prserrs *errs, enum jy_ast sect, size_t *__)
{
	char msg[] = "error: unrecognized token";
	errtkn(errs, tkns, p->tkn, msg, sizeof(msg));

	return true;
}

static bool _err_str(struct parser *p, struct jy_asts *asts,
		     struct jy_tkns *tkns, struct jy_prserrs *errs,
		     enum jy_ast sect, size_t *__)
{
	char msg[] = "error: unterminated string";
	errtkn(errs, tkns, p->tkn, msg, sizeof(msg));

	return true;
}

static bool _literal(struct parser *p, struct jy_asts *asts,
		     struct jy_tkns *tkns, struct jy_prserrs *errs,
		     enum jy_ast sect, size_t *root)
{
	enum jy_tkn type = tkns->types[p->tkn];

	switch (type) {
	case TKN_NUMBER:
		*root = addast(asts, AST_LONG, p->tkn, NULL, 0);
		break;
	case TKN_STRING:
		*root = addast(asts, AST_STRING, p->tkn, NULL, 0);
		break;
	case TKN_FALSE:
		*root = addast(asts, AST_FALSE, p->tkn, NULL, 0);
		break;
	case TKN_TRUE:
		*root = addast(asts, AST_TRUE, p->tkn, NULL, 0);
		break;

	default: {
		char msg[] = "error: invalid literal";
		errtkn(errs, tkns, p->tkn, msg, sizeof(msg));
		goto PANIC;
	}
	}

	// consume literal
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _name(struct parser *p, struct jy_asts *asts, struct jy_tkns *tkns,
		  struct jy_prserrs *errs, enum jy_ast sect, size_t *root)
{
	*root = addast(asts, AST_NAME, p->tkn, NULL, 0);
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);
}

static bool _not(struct parser *p, struct jy_asts *asts, struct jy_tkns *tkns,
		 struct jy_prserrs *errs, enum jy_ast sect, size_t *root)
{
	*root	      = addast(asts, AST_NOT, p->tkn, NULL, 0);
	size_t topast = 0;

	// consume !
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (_precedence(p, asts, tkns, errs, sect, &topast, PREC_UNARY))
		goto PANIC;

	addchild(asts, *root, topast);

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _event(struct parser *p, struct jy_asts *asts, struct jy_tkns *tkns,
		   struct jy_prserrs *errs, enum jy_ast sect, size_t *root)
{
	// consume $
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		char   msg[] = "error: expected identifier";
		size_t tkn   = find_last_tkn(tkns);
		errtkn(errs, tkns, tkn, msg, sizeof(msg));
		goto PANIC;
	}

	*root = addast(asts, AST_EVENT, p->tkn, NULL, 0);
	// consume name
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);
PANIC:
	return true;
}

static bool _grouping(struct parser *p, struct jy_asts *asts,
		      struct jy_tkns *tkns, struct jy_prserrs *errs,
		      enum jy_ast sect, size_t *root)
{
	// consume (
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (_expression(p, asts, tkns, errs, sect, root))
		goto PANIC;

	if (tkns->types[p->tkn] != TKN_RIGHT_PAREN) {
		char   msg[]   = "error: expected ')' after";
		size_t lasttkn = find_last_tkn(tkns);
		errtkn(errs, tkns, lasttkn, msg, sizeof(msg));
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _dot(struct parser *p, struct jy_asts *asts, struct jy_tkns *tkns,
		 struct jy_prserrs *errs, enum jy_ast sect, size_t *root)
{
	enum jy_ast prevtype = asts->types[*root];

	switch (prevtype) {
	case AST_NAME:
	case AST_EVENT:
		break;
	default: {
		char msg[] = "error: inappropriate accesor usage";
		errtkn(errs, tkns, p->tkn, msg, sizeof(msg));
		goto PANIC;
	}
	}

	// consume .
	size_t tkndot = p->tkn;
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		char msg[] = "error: expected identifier";
		errtkn(errs, tkns, tkndot, msg, sizeof(msg));
		goto PANIC;
	}

	size_t member = addast(asts, AST_MEMBER, p->tkn, NULL, 0);

	addchild(asts, *root, member);

	// consume identifier
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _call(struct parser *p, struct jy_asts *asts, struct jy_tkns *tkns,
		  struct jy_prserrs *errs, enum jy_ast sect, size_t *root)
{
	enum jy_ast type = asts->types[*root];

	if (type != AST_NAME) {
		char msg[] = "error: inappropriate invocation";
		errtkn(errs, tkns, p->tkn, msg, sizeof(msg));
		goto PANIC;
	}

	// consume (
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	asts->types[*root] = AST_CALL;

	enum jy_tkn param  = tkns->types[p->tkn];

	while (param != TKN_RIGHT_PAREN) {
		size_t topast = 0;

		if (_expression(p, asts, tkns, errs, sect, &topast))
			goto PANIC;

		addchild(asts, *root, topast);

		if (tkns->types[p->tkn] != TKN_COMMA)
			break;

		next(tkns, &p->src, &p->srcsz, &p->tkn);
		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

		param = tkns->types[p->tkn];
	}

	if (tkns->types[p->tkn] != TKN_RIGHT_PAREN) {
		char   msg[]   = "error: expected ')' after";
		size_t lasttkn = find_last_tkn(tkns);
		errtkn(errs, tkns, lasttkn, msg, sizeof(msg));
		goto PANIC;
	}

	// consume ')'
	next(tkns, &p->src, &p->srcsz, &p->tkn);
	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _alias(struct parser *p, struct jy_asts *asts, struct jy_tkns *tkns,
		   struct jy_prserrs *errs, size_t *left)
{
	// consume as
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		char msg[] = "error: expected identifier";
		errtkn(errs, tkns, p->tkn, msg, sizeof(msg));
		goto PANIC;
	}

	size_t alias = addast(asts, AST_ALIAS, p->tkn, NULL, 0);
	addchild(asts, alias, *left);
	*left = alias;

	// consume identifier
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);
PANIC:
	return true;
}

static bool _tilde(struct parser *p, struct jy_asts *asts, struct jy_tkns *tkns,
		   struct jy_prserrs *errs, enum jy_ast sect, size_t *root)
{
	size_t	    optkn   = p->tkn;
	size_t	    left    = *root;
	enum jy_ast leftype = asts->types[left];

	if (leftype != AST_NAME && leftype != AST_EVENT) {
		char msg[] = "error: expect a name or event before";
		errtkn(errs, tkns, optkn, msg, sizeof(msg));
		goto PANIC;
	}

	// consume ~
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	size_t	    regextkn = p->tkn;
	enum jy_tkn ttype    = tkns->types[p->tkn];

	if (ttype != TKN_REGEXP) {
		char msg[] = "error: expect a regex after";
		errtkn(errs, tkns, optkn, msg, sizeof(msg));
		goto PANIC;
	}

	// consume regexp
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	ttype = tkns->types[p->tkn];

	if (ttype != TKN_NEWLINE) {
		char msg[] = "error: invalid regex match expression";
		errtkn(errs, tkns, optkn, msg, sizeof(msg));
		goto PANIC;
	}

	size_t optast = addast(asts, AST_REGMATCH, optkn, NULL, 0);
	size_t right  = addast(asts, AST_REGEXP, regextkn, NULL, 0);
	addchild(asts, optast, left);
	addchild(asts, optast, right);

	*root = optast;

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _binary(struct parser *p, struct jy_asts *asts,
		    struct jy_tkns *tkns, struct jy_prserrs *errs,
		    enum jy_ast sect, size_t *root)
{
	enum jy_ast leftype;

	size_t left	    = *root;
	size_t right	    = 0;
	size_t optkn	    = p->tkn;

	leftype		    = asts->types[left];

	enum jy_tkn  optype = tkns->types[p->tkn];
	struct rule *oprule = rule(optype);

	switch (optype) {
	case TKN_PLUS:
		*root = addast(asts, AST_ADDITION, optkn, NULL, 0);
		break;
	case TKN_MINUS:
		*root = addast(asts, AST_SUBTRACT, optkn, NULL, 0);
		break;
	case TKN_STAR:
		*root = addast(asts, AST_MULTIPLY, optkn, NULL, 0);
		break;
	case TKN_SLASH:
		*root = addast(asts, AST_DIVIDE, optkn, NULL, 0);
		break;

	case TKN_EQUAL:
		*root = addast(asts, AST_EQUALITY, optkn, NULL, 0);
		break;
	case TKN_LESSTHAN:
		*root = addast(asts, AST_LESSER, optkn, NULL, 0);
		break;
	case TKN_GREATERTHAN:
		*root = addast(asts, AST_GREATER, optkn, NULL, 0);
		break;

	default:
		break;
	}

	// consume binary operator
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (_precedence(p, asts, tkns, errs, sect, &right, oprule->prec))
		goto PANIC;

	addchild(asts, *root, left);
	addchild(asts, *root, right);

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _precedence(struct parser *p, struct jy_asts *asts,
			struct jy_tkns *tkns, struct jy_prserrs *errs,
			enum jy_ast sect, size_t *root, enum prec rbp)
{
	enum jy_tkn  pretype	= tkns->types[p->tkn];
	struct rule *prefixrule = rule(pretype);
	parsefn_t    prefixfn	= prefixrule->prefix;

	if (prefixfn == NULL) {
		char   msg[] = "error: invalid expression";
		size_t tkn   = find_last_tkn(tkns);
		errtkn(errs, tkns, tkn, msg, sizeof(msg));
		goto PANIC;
	}

	if (prefixfn(p, asts, tkns, errs, sect, root))
		goto PANIC;

	enum prec nextprec = rule(tkns->types[p->tkn])->prec;

	while (rbp < nextprec) {
		parsefn_t infixfn = rule(tkns->types[p->tkn])->infix;

		if (infixfn(p, asts, tkns, errs, sect, root))
			goto PANIC;

		nextprec = rule(tkns->types[p->tkn])->prec;
	}

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

inline static bool _expression(struct parser *p, struct jy_asts *asts,
			       struct jy_tkns *tkns, struct jy_prserrs *errs,
			       enum jy_ast sect, size_t *topast)
{
	size_t lastast = asts->size - 1;

	if (_precedence(p, asts, tkns, errs, sect, topast, PREC_ASSIGNMENT)) {
		// clean until last valid (non inclusive)
		while (lastast + 1 < asts->size)
			popast(asts);

		goto PANIC;
	}

	return false;
PANIC:
	return true;
}

static bool _section(struct parser *p, struct jy_asts *asts,
		     struct jy_tkns *tkns, struct jy_prserrs *errs)
{
	size_t	    sectast    = asts->size - 1;
	size_t	    secttkn    = p->tkn;
	enum jy_tkn sectkntype = tkns->types[secttkn];

	switch (sectkntype) {
	case TKN_TARGET:
		asts->types[sectast] = AST_TARGET;
		break;
	case TKN_INPUT:
		asts->types[sectast] = AST_INPUT;
		break;
	case TKN_MATCH:
		asts->types[sectast] = AST_MATCH;
		break;
	case TKN_CONDITION:
		asts->types[sectast] = AST_CONDITION;
		break;
	case TKN_FIELDS:
		asts->types[sectast] = AST_FIELDS;
		break;
	default: {
		char msg[] = "error: invalid section";
		errtkn(errs, tkns, secttkn, msg, sizeof(msg));
		goto PANIC;
	}
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (tkns->types[p->tkn] != TKN_COLON) {
		char msg[] = "error: expected ':' after";
		errtkn(errs, tkns, secttkn, msg, sizeof(msg));
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);
	skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn listype = tkns->types[p->tkn];
	enum jy_ast sectype = asts->types[sectast];

	while (listype != TKN_RIGHT_BRACE && !isdecl(listype) &&
	       !issect(listype) && !ended(tkns->types, tkns->size)) {
		size_t root;

		if (_expression(p, asts, tkns, errs, sectype, &root)) {
			if (synclist(tkns, &p->src, &p->srcsz, &p->tkn))
				goto PANIC;

			goto NEXT_LIST;
		}

		addchild(asts, sectast, root);

NEXT_LIST:
		if (tkns->types[p->tkn] != TKN_NEWLINE) {
			char   msg[]   = "error: expected '\\n' before";
			size_t lasttkn = find_last_tkn(tkns);
			errtkn(errs, tkns, lasttkn, msg, sizeof(msg));
			goto PANIC;
		}

		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

		listype = tkns->types[p->tkn];
	}

CLOSING:
	return ended(tkns->types, tkns->size);

PANIC:
	// remove all node up to sectast (inclusive)
	while (sectast < asts->size)
		popast(asts);

	return true;
}

static bool _declaration(struct parser *p, struct jy_asts *asts,
			 struct jy_tkns *tkns, struct jy_prserrs *errs)
{
	size_t declast	     = asts->size - 1;
	size_t decltkn	     = p->tkn;
	enum jy_tkn decltype = tkns->types[decltkn];

	switch (decltype) {
	case TKN_RULE:
		asts->types[declast] = AST_RULE;
		break;
	case TKN_IMPORT:
		asts->types[declast] = AST_IMPORT;
		break;
	case TKN_INGRESS:
		asts->types[declast] = AST_INGRESS;
		break;
	case TKN_INCLUDE: {
		asts->types[declast] = AST_INCLUDE;
		next(tkns, &p->src, &p->srcsz, &p->tkn);
		size_t pattkn = p->tkn;

		if (tkns->types[pattkn] != TKN_STRING) {
			char msg[] = "error: expected string after";

			errtkn(errs, tkns, pattkn, msg, sizeof(msg));
		}

		size_t pathast = addast(asts, AST_PATH, pattkn, NULL, 0);
		addchild(asts, declast, pathast);

		goto PARSED;
	}

	default: {
		char msg[] = "error: invalid declaration";
		errtkn(errs, tkns, decltkn, msg, sizeof(msg));
		goto PANIC;
	}
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	size_t	    nametkn  = p->tkn;
	enum jy_tkn nametype = tkns->types[nametkn];

	if (nametype != TKN_IDENTIFIER) {
		char msg[] = "error: expected identifier after";
		errtkn(errs, tkns, decltkn, msg, sizeof(msg));
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	asts->tkns[declast] = nametkn;

	if (decltype == TKN_IMPORT)
		goto PARSED;

	if (tkns->types[p->tkn] != TKN_LEFT_BRACE) {
		char msg[] = "error: expected '{' after";
		errtkn(errs, tkns, nametkn, msg, sizeof(msg));
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn sectype = tkns->types[p->tkn];

	while (sectype != TKN_RIGHT_BRACE && !isdecl(sectype)) {
		size_t sectast = addast(asts, AST_NONE, p->tkn, NULL, 0);

		if (_section(p, asts, tkns, errs)) {
			if (syncsection(tkns, &p->src, &p->srcsz, &p->tkn))
				goto CLOSING;
			goto NEXT_SECTION;
		}

		addchild(asts, declast, sectast);

NEXT_SECTION:
		sectype = tkns->types[p->tkn];
		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);
	}

CLOSING:
	if (tkns->types[p->tkn] != TKN_RIGHT_BRACE) {
		char   msg[]   = "error: expected '}' after";
		size_t lasttkn = find_last_tkn(tkns);
		errtkn(errs, tkns, lasttkn, msg, sizeof(msg));
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

PARSED:
	return false;

PANIC:
	// remove all node up to declast (inclusive)
	while (declast < asts->size)
		popast(asts);

	return true;
}

static void _entry(struct parser *p, struct jy_asts *asts, struct jy_tkns *tkns,
		   struct jy_prserrs *errs)
{
	size_t roottkn = addtkn(tkns, TKN_NONE, 0, 0, NULL, 0);
	size_t rootast = addast(asts, AST_ROOT, roottkn, NULL, 0);

	next(tkns, &p->src, &p->srcsz, &p->tkn);
	skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

	while (!ended(tkns->types, tkns->size)) {
		size_t declast = addast(asts, AST_NONE, p->tkn, NULL, 0);

		if (_declaration(p, asts, tkns, errs)) {
			syncdecl(tkns, &p->src, &p->srcsz, &p->tkn);
			continue;
		}

		addchild(asts, rootast, declast);
		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);
	}
}

static struct rule rules[] = {
	[TKN_ERR]	  = { _err, NULL, PREC_NONE },
	[TKN_ERR_STR]	  = { _err_str, NULL, PREC_NONE },

	[TKN_LEFT_PAREN]  = { _grouping, _call, PREC_CALL },
	[TKN_RIGHT_PAREN] = { NULL, NULL, PREC_NONE },
	[TKN_LEFT_BRACE]  = { NULL, NULL, PREC_NONE },
	[TKN_RIGHT_BRACE] = { NULL, NULL, PREC_NONE },
	[TKN_DOT]	  = { NULL, _dot, PREC_CALL },
	[TKN_COMMA]	  = { NULL, NULL, PREC_NONE },
	[TKN_COLON]	  = { NULL, NULL, PREC_NONE },
	[TKN_NEWLINE]	  = { NULL, NULL, PREC_NONE },

	[TKN_RULE]	  = { NULL, NULL, PREC_NONE },
	[TKN_IMPORT]	  = { NULL, NULL, PREC_NONE },
	[TKN_INCLUDE]	  = { NULL, NULL, PREC_NONE },
	[TKN_INGRESS]	  = { NULL, NULL, PREC_NONE },

	[TKN_TARGET]	  = { NULL, NULL, PREC_NONE },
	[TKN_INPUT]	  = { NULL, NULL, PREC_NONE },
	[TKN_MATCH]	  = { NULL, NULL, PREC_NONE },
	[TKN_CONDITION]	  = { NULL, NULL, PREC_NONE },
	[TKN_FIELDS]	  = { NULL, NULL, PREC_NONE },

	[TKN_TILDE]	  = { NULL, _tilde, PREC_LAST },

	[TKN_PLUS]	  = { NULL, _binary, PREC_TERM },
	[TKN_MINUS]	  = { NULL, _binary, PREC_TERM },
	[TKN_SLASH]	  = { NULL, _binary, PREC_FACTOR },
	[TKN_STAR]	  = { NULL, _binary, PREC_FACTOR },

	[TKN_EQUAL]	  = { NULL, _binary, PREC_EQUALITY },
	[TKN_LESSTHAN]	  = { NULL, _binary, PREC_COMPARISON },
	[TKN_GREATERTHAN] = { NULL, _binary, PREC_COMPARISON },

	[TKN_AND]	  = { NULL, _binary, PREC_AND },
	[TKN_OR]	  = { NULL, _binary, PREC_OR },

	[TKN_NOT]	  = { _not, NULL, PREC_NONE },
	[TKN_ANY]	  = { NULL, NULL, PREC_NONE },
	[TKN_ALL]	  = { NULL, NULL, PREC_NONE },

	[TKN_REGEXP]	  = { NULL, NULL, PREC_NONE },
	[TKN_STRING]	  = { _literal, NULL, PREC_NONE },
	[TKN_NUMBER]	  = { _literal, NULL, PREC_NONE },
	[TKN_FALSE]	  = { _literal, NULL, PREC_NONE },
	[TKN_TRUE]	  = { _literal, NULL, PREC_NONE },

	[TKN_IDENTIFIER]  = { _name, NULL, PREC_NONE },
	[TKN_DOLLAR]	  = { _event, NULL, PREC_NONE },
	[TKN_ALIAS]	  = { NULL, NULL, PREC_NONE },

	[TKN_CUSTOM]	  = { NULL, NULL, PREC_NONE },
	[TKN_EOF]	  = { NULL, NULL, PREC_NONE },
};

static struct rule *rule(enum jy_tkn type)
{
	return &rules[type];
}

void jry_parse(const char *src, size_t length, struct jy_asts *asts,
	       struct jy_tkns *tkns, struct jy_prserrs *errs)
{
	struct parser p = {
		.src   = src,
		.srcsz = length,
	};

	errs->size = 0;
	tkns->size = 0;
	asts->size = 0;

	_entry(&p, asts, tkns, errs);
}

void jry_free_prserrs(struct jy_prserrs *errs)
{
	jry_free(errs->lines);
	jry_free(errs->ofs);
	jry_free(errs->lengths);

	for (size_t i = 0; i < errs->size; ++i) {
		jry_free(errs->lexemes[i]);
		jry_free(errs->msgs[i]);
	}

	jry_free(errs->lexemes);
	jry_free(errs->msgs);
}

void jry_free_asts(struct jy_asts *asts)
{
	jry_free(asts->types);
	jry_free(asts->tkns);

	for (size_t i = 0; i < asts->size; ++i)
		jry_free(asts->child[i]);

	jry_free(asts->child);
	jry_free(asts->childsz);
}

void jry_free_tkns(struct jy_tkns *tkns)
{
	jry_free(tkns->types);
	jry_free(tkns->lines);
	jry_free(tkns->ofs);

	for (size_t i = 0; i < tkns->size; ++i)
		jry_free(tkns->lexemes[i]);

	jry_free(tkns->lexemes);
	jry_free(tkns->lexsz);
}
