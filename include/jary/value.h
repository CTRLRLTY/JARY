#ifndef JAYVM_VALUE_H
#define JAYVM_VALUE_H

#include "jary/fnv.h"

#include <stdint.h>

typedef uint64_t jy_val_t;

struct jy_obj_str {
	char	 *str;
	size_t	  size;
	strhash_t hash;
};

jy_val_t jry_str_val(const char *str, size_t length);
jy_val_t jry_long_val(int64_t num);

struct jy_obj_str *jry_val_str(jy_val_t val);
int64_t		   jry_val_long(jy_val_t val);

#endif // JAYVM_VALUE_H