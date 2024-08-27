#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include "ast.h"
#include "token.h"

#include <stddef.h>

struct jy_prserrs {
	size_t *lines;
	// line offset array
	size_t *ofs;
	// lexeme length array
	size_t *lengths;
	char  **lexemes;
	char  **msgs;
	// total errors
	size_t	size;
};

struct jy_asts {
	enum jy_ast *types;
	// index to a tkn array
	size_t	    *tkns;
	// index to ast array
	size_t	   **child;
	// degree
	size_t	    *childsz;

	// total ast nodes
	size_t size;
};

struct jy_tkns {
	enum jy_tkn *types;
	size_t	    *lines;
	size_t	    *ofs;
	char	   **lexemes;
	size_t	    *lexsz;
	size_t	     size;
};

void jry_parse(const char *src, size_t length, struct jy_asts *asts,
	       struct jy_tkns *tkns, struct jy_prserrs *errs);

void jry_free_prserrs(struct jy_prserrs *errs);
void jry_free_asts(struct jy_asts *asts);
void jry_free_tkns(struct jy_tkns *tkns);

#endif // JAYVM_PARSER_H