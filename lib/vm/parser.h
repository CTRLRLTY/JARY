#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include "ast.h"
#include "token.h"

#include <stddef.h>

typedef struct jy_parse_errs {
	size_t *lines;
	// line offset array
	size_t *ofs;
	// lexeme length array
	size_t *lengths;
	char  **lexemes;
	char  **msgs;
	// total errors
	size_t	size;
} jy_parse_errs_t;

typedef struct jy_asts {
	jy_ast_type_t *types;
	// index to a tkn array
	size_t	      *tkns;
	// index to ast array
	size_t	     **child;
	// degree
	size_t	      *childsz;

	// total ast nodes
	size_t size;
} jy_asts_t;

typedef struct jy_tkns {
	jy_tkn_type_t *types;
	size_t	      *lines;
	size_t	      *ofs;
	char	     **lexemes;
	size_t	      *lexsz;
	size_t	       size;
} jy_tkns_t;

void jry_parse(const char *src, size_t length, jy_asts_t *asts, jy_tkns_t *tkns,
	       jy_parse_errs_t *errs, size_t *depth);

void jry_free_parse_errs(jy_parse_errs_t *errs);
void jry_free_asts(jy_asts_t *asts);
void jry_free_tkns(jy_tkns_t *tkns);

#endif // JAYVM_PARSER_H