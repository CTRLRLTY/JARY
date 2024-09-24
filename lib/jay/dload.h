#ifndef JAYVM_DLOAD_H
#define JAYVM_DLOAD_H

#include "jary/memory.h"

struct jy_defs;

int jry_module_load(const char	     *path,
		    struct jy_defs   *def,
		    struct allocator *object);

int jry_module_unload(struct jy_defs *def);

const char *jry_module_error(int err);
#endif // JAYVM_DLOAD_H
