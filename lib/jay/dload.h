#ifndef JAYVM_DLOAD_H
#define JAYVM_DLOAD_H

struct jy_defs;
struct jy_object_allocator;

int jry_module_load(const char		       *path,
		    struct jy_defs	       *def,
		    struct jy_object_allocator *object);

int jry_module_unload(struct jy_defs *def);

const char *jry_module_error(int err);
#endif // JAYVM_DLOAD_H
