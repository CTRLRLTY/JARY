#ifndef jary_memory_h
#define jary_memory_h

#include <stdlib.h>

#define jary_alloc(__size) calloc(1, (__size))
#define jary_realloc(__ptr, __size)  realloc((__ptr), (__size))
#define jary_free(__ptr) free((__ptr))

#endif // jary_memory_h