#ifndef JAYVM_DEFS_H
#define JAYVM_DEFS_H

#include "jary/common.h"
#include "jary/types.h"

#include <stdbool.h>
#include <stdint.h>

struct jy_defs {
	char	      **keys;
	union jy_value *vals;
	enum jy_ktype  *types;
	unsigned short	capacity;
	unsigned short	size;
};

bool jry_find_def(const struct jy_defs *tbl, const char *key, uint32_t *id);

__use_result int jry_add_def(struct jy_defs *tbl,
			     const char	    *key,
			     union jy_value  value,
			     enum jy_ktype   type);

void jry_free_def(struct jy_defs tbl);

#endif // JAYVM_DEFS_H
