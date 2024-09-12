#ifndef JAYVM_ERROR_H
#define JAYVM_ERROR_H

#include <assert.h>

#define jry_assert(__expr) assert((__expr));

#define ERROR_SUCCESS	   0
#define ERROR_GENERIC	   1

#endif // JAYVM_ERROR_H
