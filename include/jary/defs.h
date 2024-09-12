#ifndef JAYVM_DEFS_H
#define JAYVM_DEFS_H

#include "jary/common.h"
#include "jary/types.h"
#include "jary/value.h"

#include <stdbool.h>
#include <stddef.h>

struct jy_defs {
	char	     **keys;
	size_t	      *keysz;
	jy_val_t      *vals;
	enum jy_ktype *types;
	size_t	       seed;
	unsigned int   capacity;
	unsigned int   size;
};

bool jry_find_def(struct jy_defs *tbl,
		  const char	 *key,
		  size_t	  length,
		  size_t	 *id);

USE_RESULT int jry_add_def(struct jy_defs *tbl,
			   const char	  *key,
			   size_t	   length,
			   jy_val_t	   value,
			   enum jy_ktype   type);

void jry_free_def(struct jy_defs tbl);

#endif // JAYVM_DEFS_H
