#ifndef JAYVM_DLOAD_H
#define JAYVM_DLOAD_H

struct jy_defs;

int jry_module_load(const char *path, struct jy_defs *def);

int jry_module_unload(struct jy_defs *def);

const char *jry_module_error(int err);
#endif // JAYVM_DLOAD_H
