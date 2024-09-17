#ifndef JAYVM_MODULES_H
#define JAYVM_MODULES_H

#include "jary/object.h"

int define_function(int			module,
		    const char	       *key,
		    uint32_t		keysz,
		    struct jy_obj_func *func);

const char *error_message(int status);

#endif // JAYVM_MODULES_H
