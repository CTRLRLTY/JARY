#ifndef JAYVM_EXEC_H
#define JAYVM_EXEC_H

#include "jary/types.h"

#include <stdint.h>

int jry_exec(const union jy_value *vals,
	     const enum jy_ktype  *types,
	     const void		  *obj,
	     const uint8_t	  *codes,
	     uint32_t		   codesz);

#endif // JAYVM_EXEC_H
