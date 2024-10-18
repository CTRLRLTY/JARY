#ifndef JAYVM_EXEC_H
#define JAYVM_EXEC_H

#include <stdint.h>

struct sqlite3;
struct jy_jay;
struct jy_exec;

int jry_exec(struct sqlite3	 *db,
	     struct jy_exec	 *exec,
	     const struct jy_jay *jay);

#endif // JAYVM_EXEC_H
