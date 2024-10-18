#ifndef JAYVM_DEFS_H
#define JAYVM_DEFS_H

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

bool def_find(const struct jy_defs *tbl, const char *key, uint32_t *id);

int def_set(struct jy_defs *tbl,
	    const char	   *key,
	    union jy_value  value,
	    enum jy_ktype   type);

int def_get(struct jy_defs *tbl,
	    const char	   *key,
	    union jy_value *value,
	    enum jy_ktype  *type);

int def_add(struct jy_defs *tbl,
	    const char	   *key,
	    union jy_value  value,
	    enum jy_ktype   type);

void def_free(struct jy_defs *tbl);
void def_clear(struct jy_defs *tbl);

#endif // JAYVM_DEFS_H
