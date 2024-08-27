#ifndef JAYVM_COMPILER_H
#define JAYVM_COMPILER_H

#include "parser.h"

#include "jary/fnv.h"
#include "jary/value.h"

enum jy_opcode {
	JY_OP_PUSH8,
	JY_OP_PUSH16,
	JY_OP_PUSH32,
	JY_OP_PUSH64,

	JY_OP_JE,
	JY_OP_JNE,

	JY_OP_CMP,
	JY_OP_LT,
	JY_OP_GT,

	JY_OP_ADD,
	JY_OP_SUB,
	JY_OP_MUL,
	JY_OP_DIV,
};

enum jy_ktype {
	JY_K_UNKNOWN = -1,
	JY_K_RULE,
	JY_K_INGRESS,

	JY_K_LONG,
	JY_K_STR,
	JY_K_BOOL,
};

struct jy_funcdef {
	enum jy_ktype type;

	size_t	       paramsz;
	enum jy_ktype *ptypes;
};

struct jy_names {
	strhash_t      *hashs;
	char	      **strs;
	size_t	       *strsz;
	enum jy_ktype **types;
	size_t		size;
};

struct jy_cerrs {
	size_t size;
};

struct jy_kpool {
	jy_val_t      *vals;
	enum jy_ktype *types;
	size_t	       size;
};

struct jy_chunks {
	uint8_t *codes;
	size_t	 size;
};

struct jy_scan_ctx {
	struct jy_kpool	 pool;
	struct jy_names	 names;
	struct jy_chunks cnk;
};

void jry_compile(struct jy_asts *asts, struct jy_tkns *tkns,
		 struct jy_scan_ctx *ctx, struct jy_cerrs *errs);

void jry_free_scan_ctx(struct jy_scan_ctx *ctx);

#endif // JAYVM_COMPILER_H