#ifndef JAYVM_MEM_H
#define JAYVM_MEM_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

struct allocator {
	void	*buf;
	uint32_t size;
	uint32_t capacity;
};

static inline void *alloc_linear(uint16_t	   nmemb,
				 uint32_t	   grow,
				 struct allocator *alloc)
{
	uint32_t oldsz	= alloc->size;
	alloc->size    += nmemb;

	if (alloc->size >= alloc->capacity) {
		uint32_t newcap = alloc->capacity + grow;
		alloc->capacity = newcap;
		char *block	= (char *) realloc(alloc->buf, newcap);
		memset(block + oldsz, 0, newcap - oldsz);

		if (block == NULL)
			return NULL;

		alloc->buf = block;
	}

	return (char *) alloc->buf + oldsz;
}

static inline long memory_offset(void *from, void *to)
{
	return (char *) to - (char *) from;
}

#endif // JAYVM_MEM_H
