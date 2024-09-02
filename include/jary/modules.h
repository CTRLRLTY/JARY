#ifndef JAYVM_MODULES_H
#define JAYVM_MODULES_H

#include "jary/object.h"

struct jy_module_ctx;

int define_function(struct jy_module_ctx *ctx, const char *key, size_t keysz,
		    struct jy_obj_func *func);

#endif // JAYVM_MODULES_H