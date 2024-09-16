#ifndef JAYVM_COMPILER_H
#define JAYVM_COMPILER_H

#include "parser.h"

#include "jary/defs.h"
#include "jary/object.h"

#include <stdint.h>

enum jy_opcode {
	JY_OP_PUSH8,
	JY_OP_PUSH16,
	JY_OP_PUSH32,
	JY_OP_PUSH64,

	JY_OP_JMPT,
	JY_OP_JMPF,
	JY_OP_CALL,

	JY_OP_CMP,
	JY_OP_LT,
	JY_OP_GT,

	JY_OP_ADD,
	JY_OP_SUB,
	JY_OP_MUL,
	JY_OP_DIV,

	JY_OP_END
};

struct jy_scan_ctx {
	// module dirpath
	const char     *mdir;
	// list of modules
	int	       *modules;
	// global names
	struct jy_defs *names;
	// event array
	struct jy_defs *events;
	// code chunk array
	uint8_t	       *codes;
	// call table
	jy_funcptr_t  **call;
	// constant table
	jy_val_t       *vals;
	enum jy_ktype  *types;
	// object linear memory buffer
	void	       *obj;
	uint32_t	modulesz;
	uint32_t	objsz;
	uint32_t	valsz;
	uint32_t	eventsz;
	uint32_t	codesz;
	uint32_t	callsz;
};

void jry_compile(struct jy_asts	    *asts,
		 struct jy_tkns	    *tkns,
		 struct jy_scan_ctx *ctx,
		 struct jy_errs	    *errs);

void jry_free_scan_ctx(struct jy_scan_ctx ctx);

#endif // JAYVM_COMPILER_H
