#ifndef JAYVM_MEM_H
#define JAYVM_MEM_H

#include <stdlib.h>

#define jary_alloc(__size)	    malloc((__size))
#define jary_realloc(__ptr, __size) realloc((__ptr), (__size))
#define jary_free(__ptr)	    free((__ptr))

#endif // JAYVM_MEM_H