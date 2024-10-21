#ifndef JAYVM_PARSER_H
#define JAYVM_PARSER_H

#include <stdint.h>

struct sc_mem;
struct tkn_errs;
struct jy_asts;
struct jy_tkns;

void jry_parse(struct sc_mem   *alloc,
	       struct jy_asts  *ast,
	       struct jy_tkns  *tkns,
	       struct tkn_errs *errs,
	       const char      *src,
	       uint32_t		length);

#endif // JAYVM_PARSER_H
