#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include "ast.h"
#include "token.h"

#include <stddef.h>

struct jy_errs {
	const char  **msgs;
	unsigned int *ids;
	size_t	      size;
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

void jry_parse(const char     *src,
	       size_t	       length,
	       struct jy_asts *ast,
	       struct jy_tkns *tkns,
	       struct jy_errs *errs);

void jry_free_asts(struct jy_asts asts);
void jry_free_tkns(struct jy_tkns tkns);
void jry_free_errs(struct jy_errs errs);
#endif // JAYVM_PARSER_H
