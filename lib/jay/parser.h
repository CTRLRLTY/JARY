#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include "ast.h"
#include "token.h"

#include <stdint.h>

struct jy_errs {
	const char **msgs;
	uint32_t    *from;
	uint32_t    *to;
	uint32_t     size;
};

struct jy_asts {
	enum jy_ast *types;
	// index to a tkn array
	uint32_t    *tkns;
	// index to ast array
	uint32_t   **child;
	// degree
	uint32_t    *childsz;

	// total ast nodes
	uint32_t size;
};

struct jy_tkns {
	enum jy_tkn *types;
	uint32_t    *lines;
	uint32_t    *ofs;
	char	   **lexemes;
	uint32_t    *lexsz;
	uint32_t     size;
};

void jry_parse(const char     *src,
	       uint32_t	       length,
	       struct jy_asts *ast,
	       struct jy_tkns *tkns,
	       struct jy_errs *errs);

int jry_push_error(struct jy_errs *errs,
		   const char	  *msg,
		   uint32_t	   from,
		   uint32_t	   to);

void jry_free_asts(struct jy_asts asts);
void jry_free_tkns(struct jy_tkns tkns);
void jry_free_errs(struct jy_errs errs);

#endif // JAYVM_PARSER_H
