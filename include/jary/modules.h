#ifndef JAYVM_MODULES_H
#define JAYVM_MODULES_H

#include "jary/types.h"

#include <stdint.h>

struct jy_module;

extern int def_func(struct jy_module	*ctx,
		    const char		*key,
		    enum jy_ktype	 return_type,
		    uint8_t		 param_size,
		    const enum jy_ktype *param_types,
		    jy_funcptr_t	 func);

extern int del_func(struct jy_module *ctx, const char *key);

extern const char *error_message(int status);

#endif // JAYVM_MODULES_H
