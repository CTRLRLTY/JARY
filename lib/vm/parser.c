
#include "parser.h"

#include "scanner.h"

#include "jary/error.h"
#include "jary/memory.h"

#include <stdbool.h>
#include <string.h>

#define MAX(__a, __b) ((__b > __a) ? __b : __a)

typedef struct parser {
	const char *src;
	size_t	    srcsz;

	// current tkn
	size_t tkn;
	size_t depth;
	size_t maxdepth;
} parser_t;

typedef enum prec {
	PREC_NONE,
	PREC_ASSIGNMENT,
	PREC_OR,	 // or
	PREC_AND,	 // and
	PREC_EQUALITY,	 // == != ~
	PREC_COMPARISON, // < > <= >=
	PREC_TERM,	 // + -
	PREC_FACTOR,	 // * /
	PREC_UNARY,	 // ! -
	PREC_CALL,	 // . ()
	PREC_ALIAS,
} prec_t;

typedef bool (*parsefn_t)(parser_t *, jy_asts_t *, jy_tkns_t *,
			  jy_parse_errs_t *, size_t *);

typedef struct rule {
	parsefn_t prefix;
	parsefn_t infix;
	prec_t	  prec;
} rule_t;

static rule_t *rule(jy_tkn_type_t type);

static bool _precedence(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
			jy_parse_errs_t *errs, size_t *topast, prec_t rbp);

static bool _expression(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
			jy_parse_errs_t *errs, size_t *topast);

inline static void adderr(jy_parse_errs_t *errs, size_t line, size_t ofs,
			  const char *lexeme, size_t lexsz, const char *msg,
			  size_t msgsz)
{
	jry_mem_push(errs->lines, errs->size, line);
	jry_mem_push(errs->ofs, errs->size, ofs);
	jry_mem_push(errs->lexemes, errs->size, strndup(lexeme, lexsz));
	jry_mem_push(errs->msgs, errs->size, strndup(msg, msgsz));

	errs->size += 1;
}

inline static void errtkn(jy_parse_errs_t *errs, jy_tkns_t *tkns, size_t tknid,
			  const char *msg, size_t msgsz)
{
	jry_assert(tkns->size > tknid);

	size_t line = tkns->lines[tknid];
	size_t ofs  = tkns->ofs[tknid];
	char  *lex  = tkns->lexemes[tknid];
	size_t len  = tkns->lexsz[tknid];

	adderr(errs, line, ofs, lex, len, msg, msgsz);
}

inline static size_t addast(jy_asts_t *asts, jy_ast_type_t type, size_t tkn,
			    size_t *child, size_t childsz)
{
	jry_mem_push(asts->types, asts->size, type);
	jry_mem_push(asts->tkns, asts->size, tkn);
	jry_mem_push(asts->child, asts->size, child);
	jry_mem_push(asts->childsz, asts->size, childsz);

	return asts->size++;
}

inline static void popast(jy_asts_t *asts)
{
	jry_assert(asts->size > 0);

	size_t	 id    = asts->size - 1;
	size_t **child = &asts->child[id];

	jry_free(*child);

	*child	    = NULL;

	asts->size -= 1;
}

inline static size_t addchild(jy_asts_t *asts, size_t astid, size_t childid)
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

inline static size_t addtkn(jy_tkns_t *tkns, jy_tkn_type_t type, size_t line,
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
inline static size_t find_last_tkn(jy_tkns_t *tkns)
{
	jry_assert(tkns->size > 0);

	size_t	      tkn = tkns->size - 1;
	jy_tkn_type_t t	  = tkns->types[tkn];

	while (tkn < tkns->size && (t == TKN_NEWLINE || t == TKN_EOF)) {
		tkn--;
		t = tkns->types[tkn];
	}

	return tkn;
}

// check if section tkn
inline static bool issect(jy_tkn_type_t type)
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
inline static bool isdecl(jy_tkn_type_t type)
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

inline static bool ended(const jy_tkn_type_t *types, size_t length)
{
	return types[length - 1] == TKN_EOF;
}

// advance scanner and fill tkn
inline static void advance(jy_tkns_t *tkns, const char **src, size_t *srcsz)
{
	jy_tkn_type_t type;

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

inline static void next(jy_tkns_t *tkns, const char **src, size_t *srcsz,
			size_t *tkn)
{
	advance(tkns, src, srcsz);
	*tkn = tkns->size - 1;
}

inline static void skipnewline(jy_tkns_t *tkns, const char **src, size_t *srcsz,
			       size_t *tkn)
{
	while (tkns->types[*tkn] == TKN_NEWLINE)
		next(tkns, src, srcsz, tkn);
}

inline static bool synclist(jy_tkns_t *tkns, const char **src, size_t *srcsz,
			    size_t *tkn)
{
	jy_tkn_type_t t = tkns->types[*tkn];

	while (t != TKN_EOF && t != TKN_RIGHT_BRACE && t != TKN_NEWLINE &&
	       !isdecl(t) && !issect(t)) {
		next(tkns, src, srcsz, tkn);
		t = tkns->types[*tkn];
	}

	return t == TKN_EOF;
}

inline static bool syncsection(jy_tkns_t *tkns, const char **src, size_t *srcsz,
			       size_t *tkn)
{
	jy_tkn_type_t t = tkns->types[*tkn];

	while (t != TKN_EOF && t != TKN_RIGHT_BRACE && !isdecl(t) &&
	       !issect(t)) {
		next(tkns, src, srcsz, tkn);
		t = tkns->types[*tkn];
	}

	return t == TKN_EOF;
}

static bool syncdecl(jy_tkns_t *tkns, const char **src, size_t *srcsz,
		     size_t *tkn)
{
	jy_tkn_type_t t = tkns->types[*tkn];

	while (t != TKN_EOF && !isdecl(t)) {
		next(tkns, src, srcsz, tkn);
		t = tkns->types[*tkn];
	}

	return t == TKN_EOF;
}

static bool _err(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		 jy_parse_errs_t *errs, size_t *__)
{
	char msg[] = "error: unrecognized token";
	errtkn(errs, tkns, p->tkn, msg, sizeof(msg));

	return true;
}

static bool _err_str(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		     jy_parse_errs_t *errs, size_t *__)
{
	char msg[] = "error: unterminated string";
	errtkn(errs, tkns, p->tkn, msg, sizeof(msg));

	return true;
}

static bool _literal(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		     jy_parse_errs_t *errs, size_t *__)
{
	asts->types[asts->size - 1] = AST_LITERAL;

	return ended(tkns->types, tkns->size);
}

static bool _name(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		  jy_parse_errs_t *errs, size_t *__)
{
	asts->types[asts->size - 1] = AST_NAME;

	return ended(tkns->types, tkns->size);
}

static bool _unary(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		   jy_parse_errs_t *errs, size_t *__)
{
	size_t unast	   = asts->size - 1;
	size_t topast	   = 0;

	asts->types[unast] = AST_UNARY;

	if (_precedence(p, asts, tkns, errs, &topast, PREC_UNARY))
		goto PANIC;

	addchild(asts, unast, topast);

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _event(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		   jy_parse_errs_t *errs, size_t *__)
{
	size_t evast		 = asts->size - 1;
	asts->types[evast]	 = AST_EVENT;

	size_t		 nametkn = p->tkn;
	enum jy_tkn_type type	 = tkns->types[nametkn];

	if (type != TKN_IDENTIFIER) {
		char   msg[] = "error: expected identifier";
		size_t tkn   = find_last_tkn(tkns);
		errtkn(errs, tkns, tkn, msg, sizeof(msg));
		goto PANIC;
	}

	// consume identifier
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	asts->tkns[evast] = nametkn;

	return ended(tkns->types, tkns->size);
PANIC:
	return true;
}

static bool _dot(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		 jy_parse_errs_t *errs, size_t *__)
{
	size_t memberast	 = asts->size - 1;
	asts->types[memberast]	 = AST_MEMBER;

	size_t		 nametkn = p->tkn;
	enum jy_tkn_type type	 = tkns->types[nametkn];

	p->depth++;

	if (type != TKN_IDENTIFIER) {
		char msg[] = "error: expected identifier";
		errtkn(errs, tkns, p->tkn, msg, sizeof(msg));
		goto PANIC;
	}

	// consume identifier
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	size_t nameast = addast(asts, AST_NAME, nametkn, NULL, 0);

	addchild(asts, memberast, nameast);

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _call(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		  jy_parse_errs_t *errs, size_t *__)
{
	size_t callast	     = asts->size - 1;
	asts->types[callast] = AST_CALL;

	jy_tkn_type_t param  = tkns->types[p->tkn];

	while (param != TKN_RIGHT_PAREN) {
		size_t topast = 0;

		if (_expression(p, asts, tkns, errs, &topast))
			goto PANIC;

		addchild(asts, callast, topast);

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

static bool _binary(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		    jy_parse_errs_t *errs, size_t *__)
{
	size_t	      binast = asts->size - 1;
	jy_tkn_type_t optype = tkns->types[asts->tkns[binast]];
	rule_t	     *oprule = rule(optype);
	size_t	      topast = 0;
	asts->types[binast]  = AST_BINARY;

	if (_precedence(p, asts, tkns, errs, &topast, oprule->prec))
		goto PANIC;

	addchild(asts, binast, topast);
	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

static bool _grouping(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		      jy_parse_errs_t *errs, size_t *topast)
{
	popast(asts);

	size_t temp = 0;

	if (_expression(p, asts, tkns, errs, &temp))
		goto PANIC;

	*topast = temp;

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

static bool _precedence(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
			jy_parse_errs_t *errs, size_t *root, prec_t rbp)
{
	p->depth++;

	size_t nudtkn	     = p->tkn;

	rule_t	 *prefixrule = rule(tkns->types[nudtkn]);
	parsefn_t prefixfn   = prefixrule->prefix;

	// consume nud
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (prefixfn == NULL) {
		char   msg[] = "error: invalid expression";
		size_t tkn   = find_last_tkn(tkns);
		errtkn(errs, tkns, tkn, msg, sizeof(msg));
		goto PANIC;
	}

	*root = addast(asts, AST_NONE, nudtkn, NULL, 0);

	if (prefixfn(p, asts, tkns, errs, root))
		goto PANIC;

	prec_t nextprec = rule(tkns->types[p->tkn])->prec;

	while (rbp < nextprec) {
		size_t ledtkn = p->tkn;
		size_t ledast = addast(asts, AST_NONE, ledtkn, NULL, 0);

		// consume led
		next(tkns, &p->src, &p->srcsz, &p->tkn);

		addchild(asts, ledast, *root);

		parsefn_t infixfn = rule(tkns->types[ledtkn])->infix;

		if (infixfn(p, asts, tkns, errs, root))
			goto PANIC;

		*root	 = ledast;
		nextprec = rule(tkns->types[p->tkn])->prec;
	}

	return ended(tkns->types, tkns->size);

PANIC:
	return true;
}

inline static bool _expression(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
			       jy_parse_errs_t *errs, size_t *topast)
{
	size_t oldepth = p->depth;

	if (_precedence(p, asts, tkns, errs, topast, PREC_ASSIGNMENT))
		goto PANIC;

	p->maxdepth = MAX(p->maxdepth, p->depth);
	p->depth    = oldepth;
	return false;
PANIC:
	p->depth = oldepth;
	return true;
}

static bool _section(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		     jy_parse_errs_t *errs)
{
	// section is always at lv 2 depth
	size_t olddepth = p->depth;
	size_t sectast	= asts->size - 1;
	size_t secttkn	= p->tkn;

	if (!issect(tkns->types[secttkn])) {
		char msg[] = "error: invalid section";
		errtkn(errs, tkns, secttkn, msg, sizeof(msg));
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (tkns->types[p->tkn] != TKN_COLON) {
		char msg[] = "error: expected ':' after";
		errtkn(errs, tkns, secttkn, msg, sizeof(msg));
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);
	skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

	jy_tkn_type_t listype = tkns->types[p->tkn];

	while (listype != TKN_RIGHT_BRACE && !isdecl(listype) &&
	       !issect(listype)) {
		size_t topast;

		if (_expression(p, asts, tkns, errs, &topast)) {
			// remove failed expression
			while (sectast + 1 < asts->size)
				popast(asts);

			if (synclist(tkns, &p->src, &p->srcsz, &p->tkn))
				goto PANIC;

			goto NEXT_LIST;
		}

		p->maxdepth = MAX(p->maxdepth, p->depth);
		addchild(asts, sectast, topast);

NEXT_LIST:
		if (tkns->types[p->tkn] != TKN_NEWLINE) {
			char msg[] = "error: expected '\\n' before";
			errtkn(errs, tkns, find_last_tkn(tkns), msg,
			       sizeof(msg));
			goto PANIC;
		}

		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

		listype = tkns->types[p->tkn];
	}

CLOSING:
	p->maxdepth = MAX(p->maxdepth, p->depth);
	p->depth    = olddepth;
	return ended(tkns->types, tkns->size);

PANIC:
	p->depth = olddepth;
	// remove all node up to sectast (inclusive)
	while (sectast < asts->size)
		popast(asts);

	return true;
}

static bool _declaration(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
			 jy_parse_errs_t *errs)
{
	size_t olddepth	       = p->depth++;
	size_t declast	       = asts->size - 1;
	size_t decltkn	       = p->tkn;
	jy_tkn_type_t decltype = tkns->types[decltkn];

	switch (decltype) {
	case TKN_INCLUDE: {
		next(tkns, &p->src, &p->srcsz, &p->tkn);
		size_t pattkn = p->tkn;

		if (tkns->types[pattkn] != TKN_STRING) {
			char msg[] = "error: expected string after";

			errtkn(errs, tkns, pattkn, msg, sizeof(msg));
		}

		size_t pathast = addast(asts, AST_PATH, pattkn, NULL, 0);
		addchild(asts, declast, pathast);

		p->depth++;

		goto CLOSING;
	}

	case TKN_IMPORT:
	case TKN_RULE:
	case TKN_INGRESS:
		break;
	default: {
		char msg[] = "error: invalid declaration";
		errtkn(errs, tkns, decltkn, msg, sizeof(msg));
		goto PANIC;
	}
	}

	size_t nametkn = tkns->size;
	next(tkns, &p->src, &p->srcsz, &p->tkn);

	if (tkns->types[nametkn] != TKN_IDENTIFIER) {
		char msg[] = "error: expected identifier after";
		errtkn(errs, tkns, decltkn, msg, sizeof(msg));
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	size_t nameast = addast(asts, AST_NAME, nametkn, NULL, 0);

	addchild(asts, declast, nameast);

	p->depth++;

	if (decltype == TKN_IMPORT)
		goto CLOSING;

	if (tkns->types[p->tkn] != TKN_LEFT_BRACE) {
		char msg[] = "error: expected '{' after";
		errtkn(errs, tkns, nametkn, msg, sizeof(msg));
		goto PANIC;
	}

	next(tkns, &p->src, &p->srcsz, &p->tkn);

	skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

	jy_tkn_type_t sectype = tkns->types[p->tkn];

	while (sectype != TKN_RIGHT_BRACE && !isdecl(sectype)) {
		size_t sectast = addast(asts, AST_SECTION, p->tkn, NULL, 0);

		if (_section(p, asts, tkns, errs)) {
			if (syncsection(tkns, &p->src, &p->srcsz, &p->tkn))
				goto PANIC;
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

	p->maxdepth = MAX(p->maxdepth, p->depth);
	p->depth    = olddepth;
	return false;

PANIC:
	p->depth = olddepth;

	// remove all node up to declast (inclusive)
	while (declast < asts->size)
		popast(asts);

	return true;
}

static void _entry(parser_t *p, jy_asts_t *asts, jy_tkns_t *tkns,
		   jy_parse_errs_t *errs)
{
	size_t roottkn = addtkn(tkns, 0, 0, 0, NULL, 0);
	size_t rootast = addast(asts, AST_ROOT, roottkn, NULL, 0);

	next(tkns, &p->src, &p->srcsz, &p->tkn);
	skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);

	p->depth    = 0;
	p->maxdepth = 0;

	while (!ended(tkns->types, tkns->size)) {
		size_t declast = addast(asts, AST_DECL, p->tkn, NULL, 0);

		if (_declaration(p, asts, tkns, errs)) {
			syncdecl(tkns, &p->src, &p->srcsz, &p->tkn);
			continue;
		}

		addchild(asts, rootast, declast);
		skipnewline(tkns, &p->src, &p->srcsz, &p->tkn);
	}
}

static rule_t rules[] = {
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

	[TKN_TARGET]	  = { NULL, NULL, PREC_NONE },
	[TKN_INPUT]	  = { NULL, NULL, PREC_NONE },
	[TKN_MATCH]	  = { NULL, NULL, PREC_NONE },
	[TKN_CONDITION]	  = { NULL, NULL, PREC_NONE },
	[TKN_FIELDS]	  = { NULL, NULL, PREC_NONE },

	[TKN_RULE]	  = { NULL, NULL, PREC_NONE },
	[TKN_IMPORT]	  = { NULL, NULL, PREC_NONE },
	[TKN_INCLUDE]	  = { NULL, NULL, PREC_NONE },
	[TKN_INGRESS]	  = { NULL, NULL, PREC_NONE },

	[TKN_PLUS]	  = { NULL, _binary, PREC_TERM },
	[TKN_MINUS]	  = { NULL, _binary, PREC_TERM },
	[TKN_SLASH]	  = { NULL, _binary, PREC_FACTOR },
	[TKN_STAR]	  = { NULL, _binary, PREC_FACTOR },

	[TKN_NOT]	  = { _unary, NULL, PREC_NONE },
	[TKN_EQUAL]	  = { NULL, _binary, PREC_EQUALITY },
	[TKN_TILDE]	  = { NULL, _binary, PREC_EQUALITY },
	[TKN_LESSTHAN]	  = { NULL, _binary, PREC_COMPARISON },
	[TKN_GREATERTHAN] = { NULL, _binary, PREC_COMPARISON },
	[TKN_AND]	  = { NULL, _binary, PREC_AND },
	[TKN_OR]	  = { NULL, _binary, PREC_OR },
	[TKN_ANY]	  = { NULL, NULL, PREC_NONE },
	[TKN_ALL]	  = { NULL, NULL, PREC_NONE },

	[TKN_REGEXP]	  = { NULL, NULL, PREC_NONE },
	[TKN_STRING]	  = { _literal, NULL, PREC_NONE },
	[TKN_NUMBER]	  = { _literal, NULL, PREC_NONE },
	[TKN_FALSE]	  = { _literal, NULL, PREC_NONE },
	[TKN_TRUE]	  = { _literal, NULL, PREC_NONE },

	[TKN_IDENTIFIER]  = { _name, NULL, PREC_NONE },
	[TKN_DOLLAR]	  = { _event, NULL, PREC_NONE },
	[TKN_ALIAS]	  = { NULL, _binary, PREC_ALIAS },

	[TKN_CUSTOM]	  = { NULL, NULL, PREC_NONE },
	[TKN_EOF]	  = { NULL, NULL, PREC_NONE },
};

static rule_t *rule(jy_tkn_type_t type)
{
	return &rules[type];
}

void jry_parse(const char *src, size_t length, jy_asts_t *asts, jy_tkns_t *tkns,
	       jy_parse_errs_t *errs, size_t *depth)
{
	parser_t p = {
		.src   = src,
		.srcsz = length,
	};

	errs->size = 0;
	tkns->size = 0;
	asts->size = 0;

	_entry(&p, asts, tkns, errs);

	if (depth)
		*depth = p.maxdepth;
}

void jry_free_parse_errs(jy_parse_errs_t *errs)
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

void jry_free_asts(jy_asts_t *asts)
{
	jry_free(asts->types);
	jry_free(asts->tkns);

	for (size_t i = 0; i < asts->size; ++i)
		jry_free(asts->child[i]);

	jry_free(asts->child);
	jry_free(asts->childsz);
}

void jry_free_tkns(jy_tkns_t *tkns)
{
	jry_free(tkns->types);
	jry_free(tkns->lines);
	jry_free(tkns->ofs);

	for (size_t i = 0; i < tkns->size; ++i)
		jry_free(tkns->lexemes[i]);

	jry_free(tkns->lexemes);
	jry_free(tkns->lexsz);
}
