#ifndef JAYVM_EXEC_H
#define JAYVM_EXEC_H

#include "jary/types.h"

#include <stdint.h>
struct sqlite3;

int jry_exec(struct sqlite3	  *db,
	     const union jy_value *vals,
	     const uint8_t	  *codes,
	     uint32_t		   codesz);

#endif // JAYVM_EXEC_H
