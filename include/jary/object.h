#ifndef JAYVM_OBJECT_H
#define JAYVM_OBJECT_H

#include "jary/types.h"

#include <stdbool.h>
#include <stdint.h>

struct jy_obj_str {
	// size does not include '\0'
	uint32_t size;
	// null terminated string
	char	 cstr[];
};

struct jy_obj_func {
	enum jy_ktype  return_type;
	uint8_t	       param_size;
	enum jy_ktype *param_types;
	jy_funcptr_t   func;
};

#endif // JAYVM_OBJECT_H
