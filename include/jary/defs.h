#ifndef JAYVM_DEFS_H
#define JAYVM_DEFS_H

#include "jary/common.h"
#include "jary/types.h"

#include <stdbool.h>
#include <stdint.h>

struct jy_defs {
	char	     **keys;
	jy_val_t      *vals;
	enum jy_ktype *types;
	unsigned short capacity;
	unsigned short size;
};

bool jry_find_def(const struct jy_defs *tbl, const char *key, uint32_t *id);

__use_result int jry_add_def(struct jy_defs *tbl,
			     const char	    *key,
			     jy_val_t	     value,
			     enum jy_ktype   type);

void jry_free_def(struct jy_defs tbl);

static inline jy_val_t jry_def2v(struct jy_defs *def)
{
	return (jy_val_t) def;
}

static inline struct jy_defs *jry_v2def(jy_val_t val)
{
	return (struct jy_defs *) val;
}

#endif // JAYVM_DEFS_H
