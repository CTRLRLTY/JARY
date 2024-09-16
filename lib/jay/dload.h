#ifndef JAYVM_DLOAD_H
#define JAYVM_DLOAD_H

#include "jary/common.h"

// negative result = error
// positive return = handle to module
USE_RESULT int	jry_module_load(const char *path);
void		jry_module_unload(int module);
struct jy_defs *jry_module_def(int module);
const char     *jry_module_error(int module);
#endif // JAYVM_DLOAD_H
