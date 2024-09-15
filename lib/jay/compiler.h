#ifndef JAYVM_COMPILER_H
#define JAYVM_COMPILER_H

#include "dload.h"
#include "parser.h"

#include <stdint.h>

enum jy_opcode {
	JY_OP_PUSH8,
	JY_OP_PUSH16,
	JY_OP_PUSH32,
	JY_OP_PUSH64,

	JY_OP_JMPT,
	JY_OP_JMPF,
	JY_OP_CALL,
	JY_OP_EVENT,

	JY_OP_CMP,
	JY_OP_LT,
	JY_OP_GT,

	JY_OP_ADD,
	JY_OP_SUB,
	JY_OP_MUL,
	JY_OP_DIV,

	JY_OP_END
};

struct jy_kpool {
	jy_val_t      *vals;
	enum jy_ktype *types;
	void	      *obj;
	unsigned int   objsz;
	unsigned int   size;
};

struct jy_modules {
	const char  *dir;
	int	    *list;
	unsigned int size;
};

struct jy_chunks {
	uint8_t	    *codes;
	unsigned int size;
};

struct jy_events {
	struct jy_defs *defs;
	unsigned int	size;
};

struct jy_scan_ctx {
	struct jy_modules *modules;
	struct jy_kpool	  *pool;
	struct jy_defs	  *names;
	struct jy_events  *events;
	struct jy_chunks  *cnk;
};

void jry_compile(struct jy_asts	    *asts,
		 struct jy_tkns	    *tkns,
		 struct jy_scan_ctx *ctx,
		 struct jy_errs	    *errs);

void jry_free_scan_ctx(struct jy_scan_ctx ctx);

#endif // JAYVM_COMPILER_H
