#ifndef JAYVM_ERROR_H
#define JAYVM_ERROR_H

#include <assert.h>

#define jry_assert(__expr) assert((__expr));

enum {
	ERROR_SUCCESS = 0,
	ERROR_NOMEM,

	ERROR_OPEN_MODULE,
	ERROR_LOAD_MODULE,
};

#endif // JAYVM_ERROR_H