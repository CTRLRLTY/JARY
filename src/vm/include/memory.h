#ifndef jary_memory_h
#define jary_memory_h

#include <stdlib.h>

#define jary_alloc(size) malloc(size)
#define jary_realloc(ptr, size)  realloc(ptr, size)
#define jary_free(ptr) free(ptr)

#endif // jary_memory_h