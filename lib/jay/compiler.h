#ifndef JAYVM_COMPILER_H
#define JAYVM_COMPILER_H

#include "parser.h"

#include "jary/defs.h"
#include "jary/memory.h"

#include <stdint.h>

enum jy_opcode {
	JY_OP_PUSH8,
	JY_OP_PUSH16,
	JY_OP_PUSH32,

	JY_OP_EVENT,
	JY_OP_JMPT,
	JY_OP_JMPF,
	JY_OP_CALL,

	JY_OP_CMP,
	JY_OP_NOT,
	JY_OP_LT,
	JY_OP_GT,

	JY_OP_ADD,
	JY_OP_SUB,
	JY_OP_MUL,
	JY_OP_DIV,

	JY_OP_END
};

struct jy_jay {
	// module dirpath
	const char	*mdir;
	// global names
	struct jy_defs	*names;
	// code chunk array
	uint8_t		*codes;
	// constant table
	jy_val_t	*vals;
	enum jy_ktype	*types;
	// object linear memory buffer
	struct allocator obj;
	uint32_t	 codesz;
	uint16_t	 valsz;
};

void jry_compile(const struct jy_asts *asts,
		 const struct jy_tkns *tkns,
		 struct jy_jay	      *ctx,
		 struct jy_errs	      *errs);

void *jry_fetchobj(const struct allocator obj, jy_val_t v);

void jry_free_jay(struct jy_jay ctx);

#endif // JAYVM_COMPILER_H
