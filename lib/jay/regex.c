#include "regex.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define AST_MAX UINT16_MAX

enum {
	RGX_NEXT      = 1,
	RGX_ENDED     = 2,
	RGX_OOM	      = 3,
	RGX_INVARIANT = 5,
};

enum TKN {
	TKN_LEFT_BRACKET,
	TKN_RIGHT_BRACKET,
	TKN_LEFT_PAREN,
	TKN_RIGHT_PAREN,
	TKN_LEFT_BRACE,
	TKN_RIGHT_BRACE,

	TKN_DOT,
	TKN_CARET,
	TKN_QMARK,
	TKN_VERTBAR,
	TKN_BACKSLASH,

	TKN_PLUS,
	TKN_STAR,
	TKN_COMMA,
	TKN_DOLLAR,

	TKN_ESCAPED,

	TKN_SINGLE,

	TKN_EOF,
};

enum OPCODE {
	OP_CHAR,
	OP_SPLIT16,
	OP_SPLIT8,
	OP_JMP16,
};

enum PREC {
	PREC_NONE,
	PREC_ALTERNATION,
	PREC_CONCAT,
	PREC_REPETITION,
	PREC_LAST,
};

struct cmplr {
	struct sb_mem *codebuf;
	uint8_t	      *codes;
	uint32_t       codesz;
	// last opcode index
	uint32_t       lastop;
};

struct parser {
	const char *base;
	const char *regex;
	const char *lex;
	char	   *errmsg;
	enum TKN    type;
	uint32_t    lexsz;
	// current ast id
	rgxast_t    ast;
};

union codeview {
	uint8_t	 *u8;
	int8_t	 *i8;
	uint16_t *u16;
	int16_t	 *i16;
	int32_t	 *i32;
};

static int mkast(enum RGXAST type, char c, struct rgxast *list, rgxast_t *ast)
{
	assert(list->size + 1 < AST_MAX);

	int ret = RGX_OK;

	if (list->size + 1 >= AST_MAX)
		goto OUT_OF_MEMORY;

	jry_mem_push(list->type, list->size, type);

	if (list->type == NULL)
		goto OUT_OF_MEMORY;

	jry_mem_push(list->c, list->size, c);

	if (list->c == NULL)
		goto OUT_OF_MEMORY;

	jry_mem_push(list->child, list->size, NULL);

	if (list->child == NULL)
		goto OUT_OF_MEMORY;

	jry_mem_push(list->childsz, list->size, 0);

	if (list->childsz == NULL)
		goto OUT_OF_MEMORY;

	if (ast)
		*ast = list->size;

	list->size += 1;

	goto FINISH;

OUT_OF_MEMORY:
	ret = RGX_OOM;

FINISH:
	return ret;
}

static int addchild(rgxast_t ast, rgxast_t child, struct rgxast *list)
{
	assert(ast < list->size);
	assert(child < list->size);

	rgxast_t  *chsz	    = &list->childsz[ast];
	rgxast_t **children = &list->child[ast];

	jry_mem_push(*children, *chsz, child);

	if (*children == NULL)
		goto OUT_OF_MEMORY;

	*chsz += 1;

	return RGX_OK;

OUT_OF_MEMORY:
	return RGX_OOM;
}

static inline int expr(enum PREC      rbp,
		       struct sc_mem *alloc,
		       struct parser *p,
		       struct rgxast *asts,
		       rgxast_t	     *root);

const char *tokenize(const char *start, enum TKN *type)
{
	const char *current = start;

	if (current[0] == '\0') {
		*type = TKN_EOF;
		goto FINISH;
	}

	char c = *(current++);

	switch (c) {
	case '(':
		*type = TKN_LEFT_PAREN;
		goto FINISH;
	case ')':
		*type = TKN_RIGHT_PAREN;
		goto FINISH;
	case '{':
		*type = TKN_LEFT_BRACE;
		goto FINISH;
	case '}':
		*type = TKN_RIGHT_BRACE;
		goto FINISH;
	case '+':
		*type = TKN_PLUS;
		goto FINISH;
	case '*':
		*type = TKN_STAR;
		goto FINISH;
	case '|':
		*type = TKN_VERTBAR;
		goto FINISH;
	case '\\':
		*type = TKN_ESCAPED;
		goto FINISH;
	case '^':
		*type = TKN_CARET;
		goto FINISH;
	case '?':
		*type = TKN_QMARK;
		goto FINISH;
	case '.':
		*type = TKN_DOT;
		goto FINISH;
	case '[':
		*type = TKN_LEFT_BRACKET;
		goto FINISH;
	case ']':
		*type = TKN_RIGHT_BRACKET;
		goto FINISH;
	case ',':
		*type = TKN_COMMA;
		goto FINISH;
	case '$':
		*type = TKN_DOLLAR;
		goto FINISH;
	case '\0':
		*type = TKN_EOF;
		goto FINISH;
	}

	*type = TKN_SINGLE;

FINISH:
	return current;
}

static enum PREC tknprec(enum TKN type)
{
	switch (type) {
	case TKN_LEFT_PAREN:
	case TKN_LEFT_BRACKET:
		return PREC_LAST;
	case TKN_PLUS:
	case TKN_STAR:
	case TKN_QMARK:
		return PREC_REPETITION;
	case TKN_VERTBAR:
		return PREC_ALTERNATION;
	case TKN_ESCAPED:
	case TKN_SINGLE:
		return PREC_CONCAT;
	default:
		return PREC_NONE;
	}
}

static int next(const char **regex,
		enum TKN    *type,
		uint32_t    *lexsz,
		const char **lex)
{
	const char *start = *regex;
	const char *end	  = 0;

	end = tokenize(start, type);

	*regex = end;

	if (lexsz)
		*lexsz = end - start;

	if (lex)
		*lex = start;

	return *type == TKN_EOF ? RGX_ENDED : RGX_OK;
}

static int char_emit(struct cmplr *p, char c)
{
	if (p->codesz + 2 >= UINT32_MAX)
		return RGX_OOM;

	p->codes = sb_add(p->codebuf, 0, 2);

	if (p->codes == NULL)
		goto OUT_OF_MEMORY;

	p->codes[p->codesz]	 = OP_CHAR;
	p->codes[p->codesz + 1]	 = c;
	p->lastop		 = p->codesz;
	p->codesz		+= 2;

	return RGX_OK;

OUT_OF_MEMORY:
	return RGX_OOM;
}

static int jmp16_emit(struct cmplr *p, size_t *loc)
{
	if (p->codesz + 3 >= UINT32_MAX)
		goto OUT_OF_MEMORY;

	p->codes = sb_add(p->codebuf, 0, 3);

	if (p->codes == NULL)
		goto OUT_OF_MEMORY;

	p->codes[p->codesz]  = OP_JMP16;
	p->lastop	     = p->codesz;
	*loc		     = p->codesz + 1;
	p->codesz	    += 3;

	return RGX_OK;

OUT_OF_MEMORY:
	return RGX_OOM;
}

static int split8_emit(struct cmplr *p, size_t *left, size_t *right)
{
	if (p->codesz + 3 >= UINT32_MAX)
		goto OUT_OF_MEMORY;

	p->codes = sb_add(p->codebuf, 0, 3);

	if (p->codes == NULL)
		goto OUT_OF_MEMORY;

	p->codes[p->codesz]  = OP_SPLIT8;
	p->lastop	     = p->codesz;
	*left		     = p->codesz + 1;
	*right		     = p->codesz + 2;
	p->codesz	    += 3;

	return RGX_OK;

OUT_OF_MEMORY:
	return RGX_OOM;
}

static int split16_emit(struct cmplr *p, size_t *left, size_t *right)
{
	if (p->codesz + 5 >= UINT32_MAX)
		goto OUT_OF_MEMORY;

	p->codes = sb_add(p->codebuf, 0, 5);

	if (p->codes == NULL)
		goto OUT_OF_MEMORY;

	p->codes[p->codesz]  = OP_SPLIT16;
	p->lastop	     = p->codesz;
	*left		     = p->codesz + 1;
	*right		     = p->codesz + 3;
	p->codesz	    += 5;

	return RGX_OK;

OUT_OF_MEMORY:
	return RGX_OOM;
}

static int charset(rgxast_t	 *root,
		   struct sc_mem *alloc,
		   struct parser *p,
		   struct rgxast *asts)
{
	if (mkast(RGXAST_CHARSET, 0, asts, root) == RGX_OOM)
		goto OUT_OF_MEMORY;

	const char *current = p->regex;

	while (*current != '\0' && *current != ']') {
		char	    c	 = *current;
		enum RGXAST type = RGXAST_SINGLE;

		if (c == '\\') {
			current += 1;
			type	 = RGXAST_ESCAPE;
			c	 = *current;
		}

		rgxast_t child;

		if (mkast(type, c, asts, &child) == RGX_OOM)
			goto OUT_OF_MEMORY;

		if (addchild(*root, child, asts) == RGX_OOM)
			goto OUT_OF_MEMORY;

		current += 1;
	}

	p->regex = current;
	// consume ']'
	next(&p->regex, &p->type, &p->lexsz, &p->lex);

	if (p->type != TKN_RIGHT_BRACKET) {
		size_t ofs = current - p->base;
		sc_strfmt(alloc, &p->errmsg, "col:%lu missing ]", ofs);

		if (p->errmsg == NULL)
			goto OUT_OF_MEMORY;

		goto INVARIANT;
	}

	return RGX_NEXT;

OUT_OF_MEMORY:
	p->errmsg = "oom";
INVARIANT:
	return RGX_INVARIANT;
}

static inline int prefix(rgxast_t      *root,
			 struct sc_mem *alloc,
			 struct parser *p,
			 struct rgxast *asts)
{
	switch (p->type) {
	case TKN_EOF:
		goto ENDED;
	case TKN_LEFT_PAREN: {
		next(&p->regex, &p->type, &p->lexsz, &p->lex);

		if (expr(1, alloc, p, asts, root) == RGX_INVARIANT)
			goto INVARIANT;

		if (p->type != TKN_RIGHT_PAREN) {
			size_t	   ofs	 = p->regex - p->base;
			const char fmt[] = "col:%lu missing )";
			sc_strfmt(alloc, &p->errmsg, fmt, ofs, *p->lex, p->lex);
			goto INVARIANT;
		}

		break;
	}
	case TKN_LEFT_BRACKET:
		if (charset(root, alloc, p, asts) == RGX_INVARIANT)
			goto INVARIANT;

		break;
	case TKN_ESCAPED:
		// consume '\'
		next(&p->regex, &p->type, &p->lexsz, &p->lex);

		if (mkast(RGXAST_ESCAPE, *p->lex, asts, root) == RGX_OOM)
			goto OUT_OF_MEMORY;

		break;
	case TKN_SINGLE:
		if (mkast(RGXAST_SINGLE, *p->lex, asts, root) == RGX_OOM)
			goto OUT_OF_MEMORY;

		break;
	default: {
		size_t	   ofs	 = p->regex - p->base;
		const char fmt[] = "col:%lu invalid prefix '%c': \"%s\"";
		sc_strfmt(alloc, &p->errmsg, fmt, ofs, *p->lex, p->lex);

		if (p->errmsg == NULL)
			goto OUT_OF_MEMORY;

		goto INVARIANT;
	}
	}

	return RGX_NEXT;

OUT_OF_MEMORY:
	p->errmsg = "oom";

INVARIANT:
	return RGX_INVARIANT;
ENDED:
	return RGX_ENDED;
}

static inline int infix(rgxast_t      *root,
			struct sc_mem *alloc,
			struct parser *p,
			struct rgxast *asts)
{
	if (*root >= asts->size) {
		assert(0);
		p->errmsg = "what the frick?";
		goto INVARIANT;
	};

	rgxast_t left = *root;

	switch (p->type) {
	case TKN_EOF:
		goto ENDED;
	case TKN_VERTBAR: {
		rgxast_t right = 0;

		// consume '|'
		next(&p->regex, &p->type, &p->lexsz, &p->lex);

		if (mkast(RGXAST_OR, 0, asts, root) == RGX_OOM)
			goto OUT_OF_MEMORY;

		if (expr(tknprec(p->type), alloc, p, asts, &right) != RGX_NEXT)
			goto PANIC;

		if (addchild(*root, left, asts) == RGX_OOM)
			goto OUT_OF_MEMORY;

		if (addchild(*root, right, asts) == RGX_OOM)
			goto OUT_OF_MEMORY;

		break;
	}
	case TKN_STAR: {
		// consume '*'
		next(&p->regex, &p->type, &p->lexsz, &p->lex);

		if (mkast(RGXAST_STAR, 0, asts, root) == RGX_OOM)
			goto OUT_OF_MEMORY;

		if (addchild(*root, left, asts) == RGX_OOM)
			goto OUT_OF_MEMORY;

		break;
	}
	case TKN_PLUS: {
		// consume '+'
		next(&p->regex, &p->type, &p->lexsz, &p->lex);

		if (mkast(RGXAST_PLUS, 0, asts, root) == RGX_OOM)
			goto OUT_OF_MEMORY;

		if (addchild(*root, left, asts) == RGX_OOM)
			goto OUT_OF_MEMORY;

		break;
	}
	case TKN_QMARK: {
		// consume '?'
		next(&p->regex, &p->type, &p->lexsz, &p->lex);

		if (mkast(RGXAST_QMARK, 0, asts, root) == RGX_OOM)
			goto OUT_OF_MEMORY;

		if (addchild(*root, left, asts) == RGX_OOM)
			goto OUT_OF_MEMORY;

		break;
	}
	default: {
		rgxast_t right = 0;

		if (mkast(RGXAST_CONCAT, 0, asts, root) == RGX_OOM)
			goto OUT_OF_MEMORY;

		if (expr(PREC_CONCAT, alloc, p, asts, &right) != RGX_NEXT)
			goto PANIC;

		if (addchild(*root, left, asts) == RGX_OOM)
			goto OUT_OF_MEMORY;

		if (addchild(*root, right, asts) == RGX_OOM)
			goto OUT_OF_MEMORY;

		break;
	}
	}

	return RGX_NEXT;

OUT_OF_MEMORY:
	p->errmsg = "oom";
PANIC:
INVARIANT:
	return RGX_INVARIANT;

ENDED:
	return RGX_ENDED;
}

static inline int expr(enum PREC      rbp,
		       struct sc_mem *alloc,
		       struct parser *p,
		       struct rgxast *asts,
		       rgxast_t	     *root)
{
	switch (prefix(root, alloc, p, asts)) {
	case RGX_ENDED:
		goto ENDED;
	case RGX_NEXT:
		break;
	default:
		goto PANIC;
	}

	// consume prefix
	next(&p->regex, &p->type, &p->lexsz, &p->lex);
	enum PREC lbp = tknprec(p->type);

	while (rbp <= lbp) {
		switch (infix(root, alloc, p, asts)) {
		case RGX_ENDED:
			goto ENDED;
		case RGX_NEXT:
			break;
		default:
			goto PANIC;
		}

		lbp = tknprec(p->type);
	}

	return RGX_NEXT;
ENDED:
	return RGX_ENDED;
PANIC:
	return RGX_INVARIANT;
}

static void free_asts(struct rgxast *list)
{
	for (size_t i = 0; i < list->size; ++i)
		jry_free(list->child[i]);

	jry_free(list->c);
	jry_free(list->child);
	jry_free(list->childsz);
	jry_free(list->type);

	list->c	      = NULL;
	list->child   = NULL;
	list->childsz = NULL;
	list->type    = NULL;
	list->size    = 0;
}

int rgx_parse(struct sc_mem *alloc,
	      const char    *pattern,
	      struct rgxast *list,
	      const char   **errmsg)
{
	struct parser p = {
		.base  = pattern,
		.regex = pattern,
	};

	if (sc_reap(alloc, list, (free_t) free_asts))
		goto OUT_OF_MEMORY;

	rgxast_t root;

	if (mkast(RGXAST_ROOT, 0, list, &root) == RGX_OOM)
		goto OUT_OF_MEMORY;

	rgxast_t child;

	next(&p.regex, &p.type, &p.lexsz, &p.lex);

	switch (expr(0, alloc, &p, list, &child)) {
	case RGX_INVARIANT:
		goto INVARIANT;
	case RGX_ENDED:
	case RGX_NEXT:
		break;
	default:
		assert(0);
		p.errmsg = "what the frick";
		goto INVARIANT;
	}

	if (addchild(root, child, list) == RGX_OOM)
		goto OUT_OF_MEMORY;

	return RGX_OK;

OUT_OF_MEMORY:
	p.errmsg = "oom";

INVARIANT:
	if (errmsg)
		*errmsg = p.errmsg;

	return RGX_ERROR;
}
