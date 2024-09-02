#ifndef JAYVM_OBJECT_H
#define JAYVM_OBJECT_H

#include "jary/types.h"
#include "jary/value.h"

#include <stddef.h>

typedef jy_val_t (*jy_funcptr_t)(jy_val_t *, int);

struct jy_obj_str {
	char  *str;
	size_t size;
};

struct jy_obj_func {
	enum jy_ktype  return_type;
	enum jy_ktype *param_types;
	size_t	       param_sz;
	jy_funcptr_t   func;
};

struct jy_obj_event;

#endif // JAYVM_OBJECT_H