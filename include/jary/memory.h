#ifndef JAYVM_MEM_H
#define JAYVM_MEM_H

#include <stdint.h>
#include <stdlib.h>

#define ERROR_NOMEM		   10

#define jry_alloc(__size)	   malloc((__size))
#define jry_realloc(__ptr, __size) realloc((__ptr), (__size))
#define jry_free(__ptr)		   free((__ptr))

// calculate if sz is a power of 2
#define jry_mem_full(__sz)	   (((__sz) & ((__sz) - 1)) == 0)
// grow memory from sz
#define jry_mem_grow(__sz, __ptr)                                              \
	jry_realloc((__ptr), sizeof(*(__ptr)) * ((__sz) << 1))

// push data to array
#define jry_mem_push(__ptr, __sz, __data)                                      \
	do {                                                                   \
		if (jry_mem_full((__sz) + 1))                                  \
			(__ptr) = jry_mem_grow(((__sz) + 1), (__ptr));         \
                                                                               \
		if ((__ptr) != NULL)                                           \
			(__ptr)[(__sz)] = (__data);                            \
	} while (0)

// pop data from array
#define jry_mem_pop(__ptr, __sz, __data)                                       \
	do {                                                                   \
		*(__data) = (__ptr)[(__sz) - 1];                               \
	} while (0)

static inline long memory_offset(void *from, void *to)
{
	return (char *) to - (char *) from;
}

static inline void *memory_fetch(const void *buf, long ofs)
{
	return (char *) buf + ofs;
}

#endif // JAYVM_MEM_H
