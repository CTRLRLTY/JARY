#include "parser.h"

#include "scanner.h"

#include "jary/common.h"
#include "jary/error.h"
#include "jary/memory.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(__a, __b) ((__b > __a) ? __b : __a)

#define CASE_TKN_SECT                                                          \
TKN_INPUT:                                                                     \
	case TKN_MATCH:                                                        \
	case TKN_JUMP:                                                         \
	case TKN_CONDITION:                                                    \
	case TKN_FIELD

#define CASE_TKN_DECL                                                          \
TKN_RULE:                                                                      \
	case TKN_IMPORT:                                                       \
	case TKN_INGRESS:                                                      \
	case TKN_INCLUDE

static char msg_inv_token[]	 = "unrecognized token";
static char msg_inv_section[]	 = "invalid section";
static char msg_inv_decl[]	 = "invalid declaration";
static char msg_inv_string[]	 = "unterminated string";
static char msg_inv_literal[]	 = "invalid literal";
static char msg_inv_group[]	 = "expected ')' after";
static char msg_inv_invoc[]	 = "inappropriate invocation";
static char msg_inv_access[]	 = "inappropriate accesor usage";
static char msg_inv_expression[] = "invalid expression";
static char msg_inv_type_decl[]	 = "invalid type declaration";
static char msg_inv_regex[] =
	"expect a regex afterinvalid regex match expression";
static char msg_expect_name_or_event[] = "expect a name or event before";
static char msg_expect_regex[]	       = "expect a regex after";
static char msg_expect_ident[]	       = "expected identifier";
static char msg_expect_ident_after[]   = "expect identifier after";
static char msg_expect_eqsign[]	       = "expects = after";
static char msg_expect_type[]	       = "expect a type after";
static char msg_expect_semicolon[]     = "expected ':' after";
static char msg_expect_newline[]       = "expected '\\n' before";
static char msg_expect_open_brace[]    = "expected '{' after";
static char msg_expect_close_brace[]   = "expected '}' after";
static char msg_expect_string[]	       = "expected string after";

struct parser {
	const char *src;
	size_t	    srcsz;
	// current tkn id
	size_t	    tkn;
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

typedef bool (*parsefn_t)(struct parser *,
			  struct jy_asts *,
			  struct jy_tkns *,
			  struct jy_errs *,
			  size_t *);

struct rule {
	parsefn_t prefix;
	parsefn_t infix;
	enum prec prec;
};

static struct rule *rule(enum jy_tkn type);

static bool _precedence(struct parser  *p,
			struct jy_asts *asts,
			struct jy_tkns *tkns,
			struct jy_errs *errs,
			size_t	       *topast,
			enum prec	rbp);

static bool _expression(struct parser  *p,
			struct jy_asts *asts,
			struct jy_tkns *tkns,
			struct jy_errs *errs,
			size_t	       *topast);

static int push_err(struct jy_errs *errs, const char *msg, uint32_t id)
{
	jry_mem_push(errs->msgs, errs->size, msg);
	jry_mem_push(errs->ids, errs->size, id);

	if (errs->ids == NULL)
		return ERROR_NOMEM;

	errs->size += 1;

	return ERROR_SUCCESS;
}

USE_RESULT static inline int push_ast(struct jy_asts *asts,
				      enum jy_ast     type,
				      size_t	      tkn,
				      size_t	     *id)
{
	jry_mem_push(asts->types, asts->size, type);
	jry_mem_push(asts->tkns, asts->size, tkn);
	jry_mem_push(asts->child, asts->size, NULL);
	jry_mem_push(asts->childsz, asts->size, 0);

	if (asts->childsz == NULL)
		return ERROR_NOMEM;

	if (id != NULL)
		*id = asts->size;

	asts->size += 1;

	return ERROR_SUCCESS;
}

static inline void pop_ast(struct jy_asts *asts)
{
	jry_assert(asts->size > 0);

	size_t	 id    = asts->size - 1;
	size_t **child = &asts->child[id];

	jry_free(*child);

	*child	    = NULL;

	asts->size -= 1;
}

USE_RESULT static inline int push_child(struct jy_asts *asts,
					size_t		astid,
					size_t		childid)
{
	jry_assert(asts->size > astid);
	jry_assert(asts->size > childid);

	size_t **child	 = &asts->child[astid];
	size_t	*childsz = &asts->childsz[astid];

	if (*childsz == 0) {
		*child = jry_alloc(sizeof(**child));

		if (*child == NULL)
			return ERROR_NOMEM;

	} else {
		size_t degree = *childsz + 1;
		*child	      = jry_realloc(*child, sizeof(**child) * degree);

		if (*child == NULL)
			return ERROR_NOMEM;
	}

	(*child)[*childsz]  = childid;
	*childsz	   += 1;

	return ERROR_SUCCESS;
}

USE_RESULT static inline int push_tkn(struct jy_tkns *tkns,
				      enum jy_tkn     type,
				      size_t	      line,
				      size_t	      ofs,
				      char	     *lexeme,
				      size_t	      lexsz,
				      size_t	     *id)
{
	jry_mem_push(tkns->types, tkns->size, type);
	jry_mem_push(tkns->lines, tkns->size, line);
	jry_mem_push(tkns->ofs, tkns->size, ofs);
	jry_mem_push(tkns->lexemes, tkns->size, lexeme);
	jry_mem_push(tkns->lexsz, tkns->size, lexsz);

	if (tkns->lexsz == NULL)
		return ERROR_NOMEM;

	if (id != NULL)
		*id = tkns->size++;

	return ERROR_SUCCESS;
}

// return last non newline or eof tkn
static inline size_t find_last_tkn(struct jy_tkns *tkns)
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

static inline bool ended(const enum jy_tkn *types, size_t length)
{
	return types[length - 1] == TKN_EOF;
}

// advance scanner and fill tkn
static inline void next(struct jy_tkns *tkns,
			const char    **src,
			size_t	       *srcsz,
			size_t	       *tkn)
{
	enum jy_tkn type;

	// getting previous line
	size_t line	  = tkns->lines[tkns->size - 1];
	// getting previous ofs
	size_t ofs	  = tkns->ofs[tkns->size - 1];

	const char *start = *src;
	const char *end	  = *src;

	jry_scan(*src, *srcsz, &type, &line, &ofs, &start, &end);

	size_t read  = end - *src;
	*src	     = end;
	*srcsz	     = (*srcsz > read) ? *srcsz - read : 0;

	size_t lexsz = (end - start) + 1;
	char  *lex   = jry_alloc(lexsz);

	// you are screwed.
	if (lex == NULL)
		goto PANIC;

	memcpy(lex, start, lexsz - 1);

	lex[lexsz - 1] = '\0';

	if (push_tkn(tkns, type, line, ofs, lex, lexsz, NULL) != 0)
		goto PANIC;

	*tkn	    = tkns->size;
	tkns->size += 1;

	if (type == TKN_SPACES)
		next(tkns, src, srcsz, tkn);

	return;

PANIC:
	tkns->types[lexsz - 2] = TKN_EOF;
	return;
}

static inline void skipnewline(struct jy_tkns *tkns,
			       const char    **src,
			       size_t	      *srcsz,
			       size_t	      *tkn)
{
	while (tkns->types[*tkn] == TKN_NEWLINE)
		next(tkns, src, srcsz, tkn);
}

static inline bool synclist(struct jy_tkns *tkns,
			    const char	  **src,
			    size_t	   *srcsz,
			    size_t	   *tkn)
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
			       size_t	      *srcsz,
			       size_t	      *tkn)
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
		     size_t	    *srcsz,
		     size_t	    *tkn)
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

static bool _err(struct parser	*p,
		 struct jy_asts *UNUSED(asts),
		 struct jy_tkns *UNUSED(tkns),
		 struct jy_errs *errs,
		 size_t		*UNUSED(root))
{
	push_err(errs, msg_inv_token, p->tkn);

	return true;
}

static bool _err_str(struct parser  *p,
		     struct jy_asts *UNUSED(asts),
		     struct jy_tkns *UNUSED(tkns),
		     struct jy_errs *errs,
		     size_t	    *UNUSED(root))
{
	push_err(errs, msg_inv_string, p->tkn);

	return true;
}

static bool _literal(struct parser  *p,
		     struct jy_asts *asts,
		     struct jy_tkns *tkns,
		     struct jy_errs *errs,
		     size_t	    *root)
{
	enum jy_tkn type = tkns->types[p->tkn];

	enum jy_ast root_type;
	switch (type) {
	case TKN_NUMBER:
		root_type = AST_LONG;
		break;
	case TKN_STRING:
		root_type = AST_STRING;
		break;
	case TKN_FALSE:
		root_type = AST_FALSE;
		break;
	case TKN_TRUE:
		root_type = AST_TRUE;
		break;

	default: {
		push_err(errs, msg_inv_literal, p->tkn);
		goto PANIC;
	}
	}

	if (push_ast(asts, root_type, p->tkn, root) != 0)
		goto PANIC;

	// consume literal
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _name(struct parser	 *p,
		  struct jy_asts *asts,
		  struct jy_tkns *tkns,
		  struct jy_errs *UNUSED(errs),
		  size_t	 *root)
{
	if (push_ast(asts, AST_NAME, p->tkn, root) != 0)
		return true;

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);
}

static bool _not(struct parser	*p,
		 struct jy_asts *asts,
		 struct jy_tkns *tkns,
		 struct jy_errs *errs,
		 size_t		*root)
{
	if (push_ast(asts, AST_NOT, p->tkn, root) != 0)
		goto PANIC;

	size_t topast = 0;

	// consume !
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (_precedence(p, asts, tkns, errs, &topast, PREC_UNARY))
		goto PANIC;

	if (push_child(asts, *root, topast) != 0)
		goto PANIC;

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _event(struct parser  *p,
		   struct jy_asts *asts,
		   struct jy_tkns *tkns,
		   struct jy_errs *errs,
		   size_t	  *root)
{
	// consume $
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		size_t tkn = find_last_tkn(tkns);
		push_err(errs, msg_expect_ident, tkn);
		goto PANIC;
	}

	if (push_ast(asts, AST_EVENT, p->tkn, root) != 0)
		goto PANIC;

	// consume name
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);
PANIC:
	return true;
}

static bool _grouping(struct parser  *p,
		      struct jy_asts *asts,
		      struct jy_tkns *tkns,
		      struct jy_errs *errs,
		      size_t	     *root)
{
	// consume (
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (_expression(p, asts, tkns, errs, root))
		goto PANIC;

	if (tkns->types[p->tkn] != TKN_RIGHT_PAREN) {
		size_t tkn = find_last_tkn(tkns);
		push_err(errs, msg_inv_group, tkn);
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _call(struct parser	 *p,
		  struct jy_asts *asts,
		  struct jy_tkns *tkns,
		  struct jy_errs *errs,
		  size_t	 *root)
{
	enum jy_ast type = asts->types[*root];

	if (type != AST_NAME) {
		push_err(errs, msg_inv_invoc, p->tkn);
		goto PANIC;
	}

	// consume (
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	asts->types[*root] = AST_CALL;

	enum jy_tkn param  = tkns->types[p->tkn];

	while (param != TKN_RIGHT_PAREN) {
		size_t topast = 0;

		if (_expression(p, asts, tkns, errs, &topast))
			goto PANIC;

		if (push_child(asts, *root, topast) != 0)
			goto PANIC;

		if (tkns->types[p->tkn] != TKN_COMMA)
			break;

		next(tkns, &p->src, &p->srcsz, &p->tkn);
		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

		param = tkns->types[p->tkn];
	}

	if (tkns->types[p->tkn] != TKN_RIGHT_PAREN) {
		size_t tkn = find_last_tkn(tkns);
		push_err(errs, msg_inv_group, tkn);
		goto PANIC;
	}

	// consume ')'
	next(tkns, &p->src, &p->srcsz, &p->tkn);
	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _dot(struct parser	*p,
		 struct jy_asts *asts,
		 struct jy_tkns *tkns,
		 struct jy_errs *errs,
		 size_t		*root)
{
	enum jy_ast prevtype = asts->types[*root];
	enum jy_ast member_type;

	switch (prevtype) {
	case AST_NAME:
		member_type = AST_NAME;
		break;
	case AST_EVENT:
		member_type = AST_FIELD;
		break;
	default: {
		push_err(errs, msg_inv_access, p->tkn);
		goto PANIC;
	}
	}

	// consume .
	size_t tkndot = p->tkn;
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		push_err(errs, msg_expect_ident, tkndot);
		goto PANIC;
	}

	size_t member;

	if (push_ast(asts, member_type, p->tkn, &member) != 0)
		goto PANIC;

	if (push_child(asts, *root, member) != 0)
		goto PANIC;

	// consume identifier
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	type = tkns->types[p->tkn];

	switch (type) {
	case TKN_DOT:
		if (_dot(p, asts, tkns, errs, &member))
			goto PANIC;
		break;
	case TKN_LEFT_PAREN:
		if (_call(p, asts, tkns, errs, &member))
			goto PANIC;
		break;
	default:
		break;
	}

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _tilde(struct parser  *p,
		   struct jy_asts *asts,
		   struct jy_tkns *tkns,
		   struct jy_errs *errs,
		   size_t	  *root)
{
	size_t	    optkn   = p->tkn;
	size_t	    left    = *root;
	enum jy_ast leftype = asts->types[left];

	if (leftype != AST_NAME && leftype != AST_EVENT) {
		push_err(errs, msg_expect_name_or_event, optkn);
		goto PANIC;
	}

	// consume ~
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	size_t	    regextkn = p->tkn;
	enum jy_tkn ttype    = tkns->types[p->tkn];

	if (ttype != TKN_REGEXP) {
		push_err(errs, msg_expect_regex, optkn);
		goto PANIC;
	}

	// consume regexp
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	ttype = tkns->types[p->tkn];

	if (ttype != TKN_NEWLINE) {
		push_err(errs, msg_inv_regex, optkn);
		goto PANIC;
	}

	size_t optast;
	size_t right;

	if (push_ast(asts, AST_REGMATCH, optkn, &optast) != 0)
		goto PANIC;

	if (push_ast(asts, AST_REGEXP, regextkn, &right) != 0)
		goto PANIC;

	if (push_child(asts, optast, left) != 0)
		goto PANIC;
	if (push_child(asts, optast, right) != 0)
		goto PANIC;

	*root = optast;

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _binary(struct parser  *p,
		    struct jy_asts *asts,
		    struct jy_tkns *tkns,
		    struct jy_errs *errs,
		    size_t	   *root)
{
	size_t left	    = *root;
	size_t right	    = 0;
	size_t optkn	    = p->tkn;

	enum jy_tkn  optype = tkns->types[p->tkn];
	struct rule *oprule = rule(optype);

	enum jy_ast root_type;
	switch (optype) {
	case TKN_PLUS:
		root_type = AST_ADDITION;
		break;
	case TKN_MINUS:
		root_type = AST_SUBTRACT;
		break;
	case TKN_STAR:
		root_type = AST_MULTIPLY;
		break;
	case TKN_SLASH:
		root_type = AST_DIVIDE;
		break;
	case TKN_EQUAL:
		root_type = AST_EQUALITY;
		break;
	case TKN_LESSTHAN:
		root_type = AST_LESSER;
		break;
	case TKN_GREATERTHAN:
		root_type = AST_GREATER;
		break;

	default:
		goto PANIC;
	}

	if (push_ast(asts, root_type, optkn, root) != 0)
		goto PANIC;

	// consume binary operator
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (_precedence(p, asts, tkns, errs, &right, oprule->prec))
		goto PANIC;

	if (push_child(asts, *root, left) != 0)
		goto PANIC;

	if (push_child(asts, *root, right) != 0)
		goto PANIC;

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _precedence(struct parser  *p,
			struct jy_asts *asts,
			struct jy_tkns *tkns,
			struct jy_errs *errs,
			size_t	       *root,
			enum prec	rbp)
{
	enum jy_tkn  pretype	= tkns->types[p->tkn];
	struct rule *prefixrule = rule(pretype);
	parsefn_t    prefixfn	= prefixrule->prefix;

	if (prefixfn == NULL) {
		size_t tkn = find_last_tkn(tkns);
		push_err(errs, msg_inv_expression, tkn);
		goto PANIC;
	}

	if (prefixfn(p, asts, tkns, errs, root))
		goto PANIC;

	enum prec nextprec = rule(tkns->types[p->tkn])->prec;

	while (rbp < nextprec) {
		parsefn_t infixfn = rule(tkns->types[p->tkn])->infix;

		if (infixfn(p, asts, tkns, errs, root))
			goto PANIC;

		nextprec = rule(tkns->types[p->tkn])->prec;
	}

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static inline bool _expression(struct parser  *p,
			       struct jy_asts *asts,
			       struct jy_tkns *tkns,
			       struct jy_errs *errs,
			       size_t	      *root)
{
	size_t lastast = asts->size - 1;

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

static bool _types(struct parser  *p,
		   struct jy_asts *asts,
		   struct jy_tkns *tkns,
		   struct jy_errs *errs,
		   size_t	  *root)
{
	size_t	    lastast = asts->size - 1;
	size_t	    nametkn = p->tkn;
	enum jy_tkn type    = tkns->types[nametkn];

	if (type != TKN_IDENTIFIER) {
		size_t tkn = find_last_tkn(tkns);
		push_err(errs, msg_inv_type_decl, tkn);
		goto PANIC;
	}

	size_t left;

	if (push_ast(asts, AST_FIELD_NAME, nametkn, &left) != 0)
		goto PANIC;

	// consume name
	next(tkns, &p->src, &p->srcsz, &p->tkn);
	type	     = tkns->types[p->tkn];
	size_t eqtkn = p->tkn;

	if (type != TKN_EQUAL) {
		push_err(errs, msg_expect_eqsign, nametkn);
		goto PANIC;
	}

	if (push_ast(asts, AST_NAME_DECL, eqtkn, root) != 0)
		goto PANIC;

	// consume =
	next(tkns, &p->src, &p->srcsz, &p->tkn);
	type = tkns->types[p->tkn];

	enum jy_ast right_type;
	switch (type) {
	case TKN_LONG_TYPE:
		right_type = AST_LONG_TYPE;
		break;
	case TKN_STRING_TYPE:
		right_type = AST_STR_TYPE;
		break;
	default: {
		push_err(errs, msg_expect_type, eqtkn);
		goto PANIC;
	}
	}

	size_t right;
	if (push_ast(asts, right_type, p->tkn, &right) != 0)
		goto PANIC;

	if (push_child(asts, *root, left) != 0)
		goto PANIC;

	if (push_child(asts, *root, right) != 0)
		goto PANIC;

	// consume type
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return false;

PANIC:
	// clean until last valid (non inclusive)
	while (lastast + 1 < asts->size)
		pop_ast(asts);

	return true;
}

static bool _section(struct parser  *p,
		     struct jy_asts *asts,
		     struct jy_tkns *tkns,
		     struct jy_errs *errs,
		     enum jy_ast decltype)
{
	size_t	    sectast    = asts->size - 1;
	size_t	    secttkn    = p->tkn;
	enum jy_tkn sectkntype = tkns->types[secttkn];

	// consume section
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	bool (*listfn)(struct parser *, struct jy_asts *, struct jy_tkns *,
		       struct jy_errs *, size_t *);

	switch (sectkntype) {
	case TKN_JUMP:
		if (decltype != AST_RULE_DECL)
			goto INVALID_SECTION;

		asts->types[sectast] = AST_JUMP_SECT;
		listfn		     = _expression;
		break;
	case TKN_INPUT:
		if (decltype != AST_INGRESS_DECL)
			goto INVALID_SECTION;

		asts->types[sectast] = AST_INPUT_SECT;
		break;
	case TKN_MATCH:
		if (decltype != AST_RULE_DECL)
			goto INVALID_SECTION;

		asts->types[sectast] = AST_MATCH_SECT;
		listfn		     = _expression;
		break;
	case TKN_CONDITION:
		if (decltype != AST_RULE_DECL)
			goto INVALID_SECTION;

		asts->types[sectast] = AST_CONDITION_SECT;
		listfn		     = _expression;
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
		push_err(errs, msg_expect_semicolon, secttkn);
		goto PANIC;
	}

	// consume colon
	next(tkns, &p->src, &p->srcsz, &p->tkn);
	skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn listype = tkns->types[p->tkn];

	for (;;) {
		switch (listype) {
		case CASE_TKN_DECL:
		case CASE_TKN_SECT:
		case TKN_RIGHT_BRACE:
			goto CLOSING;
		case TKN_EOF:
			goto PANIC;
		default:
			break;
		}

		size_t root;

		bool needsync = listfn(p, asts, tkns, errs, &root);

		if (needsync) {
			if (synclist(tkns, &p->src, &p->srcsz, &p->tkn))
				goto PANIC;
		} else {
			if (push_child(asts, sectast, root) != 0)
				goto PANIC;
		}

		if (tkns->types[p->tkn] != TKN_NEWLINE) {
			size_t tkn = find_last_tkn(tkns);
			push_err(errs, msg_expect_newline, tkn);
			goto PANIC;
		}

		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

		listype = tkns->types[p->tkn];
	}

CLOSING:
	return false;

INVALID_SECTION: {
	push_err(errs, msg_inv_section, secttkn);
}
PANIC:
	// remove all node up to sectast (inclusive)
	while (sectast < asts->size)
		pop_ast(asts);

	return true;
}

static bool _block(struct parser  *p,
		   struct jy_asts *asts,
		   struct jy_tkns *tkns,
		   struct jy_errs *errs,
		   size_t	   declast)
{
	enum jy_ast decl = asts->types[declast];
	enum jy_tkn tkn	 = tkns->types[p->tkn];

	if (tkn != TKN_LEFT_BRACE) {
		size_t tkn = asts->tkns[declast];
		push_err(errs, msg_expect_open_brace, tkn);
		goto PANIC;
	}

	// consume {
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn sectype = tkns->types[p->tkn];

	for (;;) {
		switch (sectype) {
		case CASE_TKN_DECL:
		case TKN_RIGHT_BRACE:
			goto CLOSING;
		case TKN_EOF:
			goto PANIC;
		default:
			break;
		}

		size_t sectast;

		if (push_ast(asts, AST_NONE, p->tkn, &sectast) != 0)
			goto PANIC;

		if (_section(p, asts, tkns, errs, decl)) {
			if (syncsection(tkns, &p->src, &p->srcsz, &p->tkn))
				goto CLOSING;
			goto NEXT_SECTION;
		}

		if (push_child(asts, declast, sectast) != 0)
			goto PANIC;

NEXT_SECTION:
		sectype = tkns->types[p->tkn];
		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);
	}

CLOSING:
	if (tkns->types[p->tkn] != TKN_RIGHT_BRACE) {
		size_t tkn = find_last_tkn(tkns);
		push_err(errs, msg_expect_close_brace, tkn);
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

static inline bool _identifier(struct parser  *p,
			       struct jy_asts *asts,
			       struct jy_tkns *tkns,
			       struct jy_errs *errs,
			       size_t	       ast)
{
	size_t	    nametkn  = p->tkn;
	enum jy_tkn nametype = tkns->types[nametkn];

	if (nametype != TKN_IDENTIFIER) {
		push_err(errs, msg_expect_ident_after, asts->tkns[ast]);
		goto PANIC;
	}

	// consume name
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	asts->tkns[ast] = nametkn;

	return false;
PANIC:
	return true;
}

static bool _declstmt(struct parser  *p,
		      struct jy_asts *asts,
		      struct jy_tkns *tkns,
		      struct jy_errs *errs)
{
	size_t declast	     = asts->size - 1;
	size_t decltkn	     = p->tkn;
	enum jy_tkn decltype = tkns->types[decltkn];

	// consume decl tkn
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	switch (decltype) {
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
		size_t pattkn	     = p->tkn;

		if (tkns->types[pattkn] != TKN_STRING) {
			push_err(errs, msg_expect_string, pattkn);
			goto PANIC;
		}

		// consume path token
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		size_t pathast;

		if (push_ast(asts, AST_PATH, pattkn, &pathast) != 0)
			goto PANIC;

		if (push_child(asts, declast, pathast) != 0)
			goto PANIC;

		break;
	}

	default: {
		push_err(errs, msg_inv_decl, decltkn);
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

USE_RESULT static int _entry(struct parser  *p,
			     struct jy_asts *asts,
			     struct jy_tkns *tkns,
			     struct jy_errs *errs)
{
	size_t roottkn;

	int status = push_tkn(tkns, TKN_NONE, 0, 0, NULL, 0, &roottkn);

	if (status != 0)
		return status;

	size_t rootast;

	status = push_ast(asts, AST_ROOT, roottkn, &rootast);

	if (status != 0)
		return status;

	next(tkns, &p->src, &p->srcsz, &p->tkn);
	skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

	while (!ended(tkns->types, tkns->size)) {
		size_t declast;

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

		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);
	}

	return ERROR_SUCCESS;
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
	[TKN_SPACES]	  = { NULL, NULL, PREC_NONE },

	[TKN_RULE]	  = { NULL, NULL, PREC_NONE },
	[TKN_IMPORT]	  = { NULL, NULL, PREC_NONE },
	[TKN_INCLUDE]	  = { NULL, NULL, PREC_NONE },
	[TKN_INGRESS]	  = { NULL, NULL, PREC_NONE },

	[TKN_JUMP]	  = { NULL, NULL, PREC_NONE },
	[TKN_INPUT]	  = { NULL, NULL, PREC_NONE },
	[TKN_MATCH]	  = { NULL, NULL, PREC_NONE },
	[TKN_CONDITION]	  = { NULL, NULL, PREC_NONE },
	[TKN_FIELD]	  = { NULL, NULL, PREC_NONE },

	[TKN_LONG_TYPE]	  = { NULL, NULL, PREC_NONE },
	[TKN_STRING_TYPE] = { NULL, NULL, PREC_NONE },

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

void jry_free_asts(struct jy_asts asts)
{
	jry_free(asts.types);
	jry_free(asts.tkns);

	for (size_t i = 0; i < asts.size; ++i)
		jry_free(asts.child[i]);

	jry_free(asts.child);
	jry_free(asts.childsz);
}

void jry_free_tkns(struct jy_tkns tkns)
{
	jry_free(tkns.types);
	jry_free(tkns.lines);
	jry_free(tkns.ofs);

	for (size_t i = 0; i < tkns.size; ++i)
		jry_free(tkns.lexemes[i]);

	jry_free(tkns.lexemes);
	jry_free(tkns.lexsz);
}

void jry_parse(const char     *src,
	       size_t	       length,
	       struct jy_asts *asts,
	       struct jy_tkns *tkns,
	       struct jy_errs *errs)
{
	struct parser p = {
		.src   = src,
		.srcsz = length,
	};

	if (_entry(&p, asts, tkns, errs) != 0) {
		jry_free_asts(*asts);
		jry_free_tkns(*tkns);
		jry_free_errs(*errs);
	}
}

void jry_free_errs(struct jy_errs errs)
{
	jry_free(errs.msgs);
	jry_free(errs.ids);
}
