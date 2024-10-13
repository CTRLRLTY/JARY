#include "parser.h"

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
	case TKN_FIELD

#define CASE_TKN_DECL                                                          \
TKN_RULE:                                                                      \
	case TKN_IMPORT:                                                       \
	case TKN_INGRESS:                                                      \
	case TKN_INCLUDE

static const char msg_inv_token[]	     = "unrecognized token";
static const char msg_inv_section[]	     = "invalid section";
static const char msg_inv_decl[]	     = "invalid declaration";
static const char msg_inv_string[]	     = "unterminated string";
static const char msg_inv_literal[]	     = "invalid literal";
static const char msg_inv_invoc[]	     = "inappropriate invocation";
static const char msg_inv_access[]	     = "inappropriate accesor usage";
static const char msg_inv_expression[]	     = "invalid expression";
static const char msg_inv_type_decl[]	     = "invalid type declaration";
static const char msg_inv_regex[]	     = "invalid regex match expression";
static const char msg_expect_name_or_event[] = "not a name or event";
static const char msg_expect_regex[]	     = "not a regex";
static const char msg_expect_ident[]	     = "not an identifier";
static const char msg_expect_eqsign[]	     = "expected '='";
static const char msg_expect_type[]	     = "not a type";
static const char msg_expect_semicolon[]     = "missing ':'";
static const char msg_expect_newline[]	     = "missing newline '\\n'";
static const char msg_expect_open_brace[]    = "missing '{'";
static const char msg_expect_close_brace[]   = "missing '}'";
static const char msg_expect_paren_close[]   = "missing ')'";
static const char msg_expect_string[]	     = "expected string";
static const char msg_args_limit[]	     = "too many arguments";

struct parser {
	const char *src;
	uint32_t    srcsz;
	// current tkn id
	uint32_t    tkn;
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
			  uint32_t *);

struct rule {
	parsefn_t prefix;
	parsefn_t infix;
	enum prec prec;
};

static inline struct rule *rule(enum jy_tkn type);

static bool _precedence(struct parser  *p,
			struct jy_asts *asts,
			struct jy_tkns *tkns,
			struct jy_errs *errs,
			uint32_t       *topast,
			enum prec	rbp);

static bool _expression(struct parser  *p,
			struct jy_asts *asts,
			struct jy_tkns *tkns,
			struct jy_errs *errs,
			uint32_t       *topast);

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

	uint32_t lexsz;
	char	*lex;

	switch (type) {
	case TKN_STRING:
		// -2 to not include  \"\"
		lexsz = read - 2;
		lex   = jry_alloc(lexsz + 1);

		// you are screwed.
		if (lex == NULL)
			goto PANIC;

		// +1 to skip \" prefix
		memcpy(lex, start + 1, lexsz);

		lex[lexsz] = '\0';
		*tkn	   = tkns->size;

		break;
	case TKN_NEWLINE:
		line += read - 1;
		// INTENTIONAL FALLTHROUGH
	default:
		lexsz = read;
		lex   = jry_alloc(lexsz + 1);

		// you are screwed.
		if (lex == NULL)
			goto PANIC;

		memcpy(lex, start, lexsz);

		lex[lexsz] = '\0';
		*tkn	   = tkns->size;
		break;
	}

	if (push_tkn(tkns, type, line, ofs, lex, lexsz) != 0)
		goto PANIC;

	if (type == TKN_SPACES)
		next(tkns, src, srcsz, tkn);

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

static bool _err(struct parser	*p,
		 struct jy_asts *__unused(asts),
		 struct jy_tkns *__unused(tkns),
		 struct jy_errs *errs,
		 uint32_t	*__unused(root))
{
	jry_push_error(errs, msg_inv_token, p->tkn, p->tkn);

	return true;
}

static bool _err_str(struct parser  *p,
		     struct jy_asts *__unused(asts),
		     struct jy_tkns *__unused(tkns),
		     struct jy_errs *errs,
		     uint32_t	    *__unused(root))
{
	jry_push_error(errs, msg_inv_string, p->tkn, p->tkn);

	return true;
}

static bool _literal(struct parser  *p,
		     struct jy_asts *asts,
		     struct jy_tkns *tkns,
		     struct jy_errs *errs,
		     uint32_t	    *root)
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
		jry_push_error(errs, msg_inv_literal, p->tkn, p->tkn);
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
		  struct jy_errs *__unused(errs),
		  uint32_t	 *root)
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
		 uint32_t	*root)
{
	if (push_ast(asts, AST_NOT, p->tkn, root) != 0)
		goto PANIC;

	uint32_t topast = 0;

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
		   uint32_t	  *root)
{
	uint32_t dlrtkn = p->tkn;
	// consume $
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		jry_push_error(errs, msg_expect_ident, dlrtkn, p->tkn);
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
		      uint32_t	     *root)
{
	uint32_t grptkn = p->tkn;
	// consume (
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (_expression(p, asts, tkns, errs, root))
		goto PANIC;

	if (tkns->types[p->tkn] != TKN_RIGHT_PAREN) {
		jry_push_error(errs, msg_expect_paren_close, grptkn, p->tkn);
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
		  uint32_t	 *root)
{
	uint32_t    left    = *root;
	enum jy_ast type    = asts->types[left];
	uint32_t    nametkn = p->tkn;
	if (type != AST_ACCESS) {
		jry_push_error(errs, msg_inv_invoc, p->tkn, p->tkn);
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
			jry_push_error(errs, msg_args_limit, nametkn, p->tkn);
			goto PANIC;
		}

		if (_expression(p, asts, tkns, errs, &topast))
			goto PANIC;

		if (push_child(asts, *root, topast) != 0)
			goto PANIC;

		if (tkns->types[p->tkn] != TKN_COMMA)
			break;

		next(tkns, &p->src, &p->srcsz, &p->tkn);

		if (tkns->types[p->tkn] == TKN_NEWLINE)
			next(tkns, &p->src, &p->srcsz, &p->tkn);

		param = tkns->types[p->tkn];
	}

	if (tkns->types[p->tkn] != TKN_RIGHT_PAREN) {
		jry_push_error(errs, msg_expect_paren_close, nametkn, p->tkn);
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
		 uint32_t	*root)
{
	uint32_t    right;
	uint32_t    left  = *root;
	enum jy_ast rtype = asts->types[left];

	switch (asts->types[left]) {
	case AST_NAME:
	case AST_EVENT:
		break;
	default: {
		jry_push_error(errs, msg_inv_access, p->tkn, p->tkn);
		goto PANIC;
	}
	}

	// set root to .
	if (push_ast(asts, AST_ACCESS, p->tkn, root) != 0)
		goto PANIC;

	// consume .
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		jry_push_error(errs, msg_expect_ident, asts->tkns[*root],
			       p->tkn);
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

static bool _tilde(struct parser  *p,
		   struct jy_asts *asts,
		   struct jy_tkns *tkns,
		   struct jy_errs *errs,
		   uint32_t	  *root)
{
	uint32_t    optkn   = p->tkn;
	uint32_t    left    = *root;
	uint32_t    lefttkn = asts->tkns[*root];
	enum jy_ast leftype = asts->types[left];

	if (leftype != AST_NAME && leftype != AST_EVENT) {
		jry_push_error(errs, msg_expect_name_or_event, lefttkn, p->tkn);
		goto PANIC;
	}

	// consume ~
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	uint32_t    regextkn = p->tkn;
	enum jy_tkn ttype    = tkns->types[p->tkn];

	if (ttype != TKN_REGEXP) {
		jry_push_error(errs, msg_expect_regex, lefttkn, regextkn);
		goto PANIC;
	}

	// consume regexp
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	ttype = tkns->types[p->tkn];

	if (ttype != TKN_NEWLINE) {
		jry_push_error(errs, msg_inv_regex, lefttkn, p->tkn);
		goto PANIC;
	}

	uint32_t optast;
	uint32_t right;

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
		    uint32_t	   *root)
{
	uint32_t left  = *root;
	uint32_t right = 0;
	uint32_t optkn = p->tkn;

	enum jy_tkn  optype = tkns->types[p->tkn];
	struct rule *oprule = rule(optype);

	enum jy_ast root_type;
	switch (optype) {
	case TKN_PLUS:
		root_type = AST_ADDITION;
		break;
	case TKN_CONCAT:
		root_type = AST_CONCAT;
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
	case TKN_AND:
		root_type = AST_AND;
		break;
	case TKN_OR:
		root_type = AST_OR;
		break;
	case TKN_JOINX:
		root_type = AST_JOINX;
		break;
	case TKN_EXACT:
		root_type = AST_EXACT;
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
			uint32_t       *root,
			enum prec	rbp)
{
	enum jy_tkn  pretype	= tkns->types[p->tkn];
	struct rule *prefixrule = rule(pretype);
	parsefn_t    prefixfn	= prefixrule->prefix;

	if (prefixfn == NULL) {
		jry_push_error(errs, msg_inv_expression, asts->tkns[*root],
			       p->tkn);
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
			       uint32_t	      *root)
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

static bool _types(struct parser  *p,
		   struct jy_asts *asts,
		   struct jy_tkns *tkns,
		   struct jy_errs *errs,
		   uint32_t	  *root)
{
	uint32_t    sectast = asts->size - 1;
	uint32_t    sectkn  = asts->tkns[sectast];
	uint32_t    nametkn = p->tkn;
	enum jy_tkn type    = tkns->types[nametkn];

	if (type != TKN_IDENTIFIER) {
		jry_push_error(errs, msg_inv_type_decl, sectkn, nametkn);
		goto PANIC;
	}

	uint32_t left;

	if (push_ast(asts, AST_EVENT_MEMBER_NAME, nametkn, &left) != 0)
		goto PANIC;

	// consume name
	next(tkns, &p->src, &p->srcsz, &p->tkn);
	type	       = tkns->types[p->tkn];
	uint32_t eqtkn = p->tkn;

	if (type != TKN_EQUAL) {
		jry_push_error(errs, msg_expect_eqsign, sectkn, eqtkn);
		goto PANIC;
	}

	if (push_ast(asts, AST_EVENT_MEMBER_DECL, eqtkn, root) != 0)
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
		jry_push_error(errs, msg_expect_type, sectkn, p->tkn);
		goto PANIC;
	}
	}

	uint32_t right;
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
	while (sectast + 1 < asts->size)
		pop_ast(asts);

	return true;
}

static bool _section(struct parser  *p,
		     struct jy_asts *asts,
		     struct jy_tkns *tkns,
		     struct jy_errs *errs,
		     uint32_t	     declast)
{
	enum jy_ast decltype   = asts->types[declast];
	uint32_t    sectast    = asts->size - 1;
	uint32_t    secttkn    = p->tkn;
	enum jy_tkn sectkntype = tkns->types[secttkn];

	// consume section
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	bool (*listfn)(struct parser *, struct jy_asts *, struct jy_tkns *,
		       struct jy_errs *, uint32_t *);

	listfn = NULL;

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
		goto INVALID_SECTION;
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
		jry_push_error(errs, msg_expect_semicolon, secttkn, p->tkn);
		goto PANIC;
	}

	// consume colon
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (tkns->types[p->tkn] == TKN_NEWLINE)
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

		uint32_t root;

		if (listfn(p, asts, tkns, errs, &root)) {
			if (synclist(tkns, &p->src, &p->srcsz, &p->tkn))
				goto PANIC;
		} else {
			if (push_child(asts, sectast, root) != 0)
				goto PANIC;
		}

		if (tkns->types[p->tkn] != TKN_NEWLINE) {
			jry_push_error(errs, msg_expect_newline, secttkn,
				       p->tkn);
			goto PANIC;
		}

		// consume NEWLINE
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		listype = tkns->types[p->tkn];
	}

FINISH:
	return false;

INVALID_SECTION: {
	jry_push_error(errs, msg_inv_section, asts->tkns[declast], p->tkn);
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
		   uint32_t	   declast)
{
	uint32_t decltkn = asts->tkns[declast];

	if (tkns->types[p->tkn] != TKN_LEFT_BRACE) {
		jry_push_error(errs, msg_expect_open_brace, decltkn, p->tkn);
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
		jry_push_error(errs, msg_expect_close_brace, decltkn, p->tkn);
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
			       uint32_t	       ast)
{
	enum jy_tkn type = tkns->types[p->tkn];

	if (type != TKN_IDENTIFIER) {
		jry_push_error(errs, msg_expect_ident, asts->tkns[ast], p->tkn);
		goto PANIC;
	}

	asts->tkns[ast] = p->tkn;
	// consume name
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	return false;
PANIC:
	return true;
}

static bool _declstmt(struct parser  *p,
		      struct jy_asts *asts,
		      struct jy_tkns *tkns,
		      struct jy_errs *errs)
{
	uint32_t declast     = asts->size - 1;
	uint32_t decltkn     = p->tkn;
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
		uint32_t pattkn	     = p->tkn;

		if (tkns->types[pattkn] != TKN_STRING) {
			jry_push_error(errs, msg_expect_string, decltkn,
				       pattkn);
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
		jry_push_error(errs, msg_inv_decl, decltkn, decltkn);
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

static int _entry(struct parser	 *p,
		  struct jy_asts *asts,
		  struct jy_tkns *tkns,
		  struct jy_errs *errs)
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

	if (tkns->types[p->tkn] == TKN_NEWLINE)
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

		if (tkns->types[p->tkn] == TKN_NEWLINE)
			next(tkns, &p->src, &p->srcsz, &p->tkn);
	}

	return 0;
}

static struct rule rules[TOTAL_TKN_TYPES] = {
	[TKN_ERR]     = { _err, NULL, PREC_NONE },
	[TKN_ERR_STR] = { _err_str, NULL, PREC_NONE },

	[TKN_LEFT_PAREN] = { _grouping, _call, PREC_CALL },
	[TKN_DOT]	 = { NULL, _dot, PREC_CALL },

	[TKN_TILDE] = { NULL, _tilde, PREC_LAST },

	[TKN_PLUS]   = { NULL, _binary, PREC_TERM },
	[TKN_CONCAT] = { NULL, _binary, PREC_TERM },
	[TKN_MINUS]  = { NULL, _binary, PREC_TERM },
	[TKN_SLASH]  = { NULL, _binary, PREC_FACTOR },
	[TKN_STAR]   = { NULL, _binary, PREC_FACTOR },

	[TKN_JOINX] = { NULL, _binary, PREC_CALL - 1 },
	[TKN_EXACT] = { NULL, _binary, PREC_EQUALITY },

	[TKN_EQUAL]	  = { NULL, _binary, PREC_EQUALITY },
	[TKN_LESSTHAN]	  = { NULL, _binary, PREC_COMPARISON },
	[TKN_GREATERTHAN] = { NULL, _binary, PREC_COMPARISON },

	[TKN_AND] = { NULL, _binary, PREC_AND },
	[TKN_OR]  = { NULL, _binary, PREC_OR },

	[TKN_NOT] = { _not, NULL, PREC_NONE },

	[TKN_STRING] = { _literal, NULL, PREC_NONE },
	[TKN_NUMBER] = { _literal, NULL, PREC_NONE },
	[TKN_FALSE]  = { _literal, NULL, PREC_NONE },
	[TKN_TRUE]   = { _literal, NULL, PREC_NONE },

	[TKN_IDENTIFIER] = { _name, NULL, PREC_NONE },
	[TKN_DOLLAR]	 = { _event, NULL, PREC_NONE },
};

static inline struct rule *rule(enum jy_tkn type)
{
	return &rules[type];
}

void free_asts(struct jy_asts *asts)
{
	jry_free(asts->types);
	jry_free(asts->tkns);

	for (uint32_t i = 0; i < asts->size; ++i)
		jry_free(asts->child[i]);

	jry_free(asts->child);
	jry_free(asts->childsz);
}

void free_tkns(struct jy_tkns *tkns)
{
	jry_free(tkns->types);
	jry_free(tkns->lines);
	jry_free(tkns->ofs);

	for (uint32_t i = 0; i < tkns->size; ++i)
		jry_free(tkns->lexemes[i]);

	jry_free(tkns->lexemes);
	jry_free(tkns->lexsz);
}

void free_errs(struct jy_errs *errs)
{
	jry_free(errs->msgs);
	jry_free(errs->from);
	jry_free(errs->to);
}

void jry_parse(struct sc_mem  *alloc,
	       struct jy_asts *ast,
	       struct jy_tkns *tkns,
	       struct jy_errs *errs,
	       const char     *src,
	       uint32_t	       length)
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

int jry_push_error(struct jy_errs *errs,
		   const char	  *msg,
		   uint32_t	   from,
		   uint32_t	   to)
{
	jry_mem_push(errs->msgs, errs->size, msg);
	jry_mem_push(errs->from, errs->size, from);
	jry_mem_push(errs->to, errs->size, to);

	if (errs->from == NULL)
		return -1;

	errs->size += 1;

	return 0;
}
