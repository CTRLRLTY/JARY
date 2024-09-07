#ifndef JAYVM_MODULES_H
#define JAYVM_MODULES_H

#include "jary/common.h"
#include "jary/defs.h"

#define ERROR_OPEN_MODULE 21
#define ERROR_LOAD_MODULE 22

struct jy_module_ctx;

USE_RESULT
int jry_module_load(const char *path, struct jy_defs **defs,
		    struct jy_module_ctx **ctx);

USE_RESULT
int jry_module_unload(struct jy_module_ctx *ctx);

#endif // JAYVM_MODULES_H