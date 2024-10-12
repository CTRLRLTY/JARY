#ifndef JAYVM_MEM_H
#define JAYVM_MEM_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
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

struct sc_mem {
	void	      *buf;
	struct sc_mem *back;
};

static inline void *sc_alloc(struct sc_mem *alloc, uint16_t nmemb)
{
	void *block = calloc(nmemb, 1);

	if (block == NULL)
		return NULL;

	struct sc_mem *temp = alloc->back;
	alloc->back	    = calloc(sizeof *alloc, 1);

	if (alloc->back == NULL)
		return NULL;

	alloc->back->buf  = block;
	alloc->back->back = temp;

	return block;
}

static inline int sc_strfmt(struct sc_mem *alloc,
			    char	 **str,
			    const char	  *fmt,
			    ...)
{
	va_list arg;

	va_start(arg, fmt);

	int sz		    = vasprintf(str, fmt, arg);

	struct sc_mem *temp = alloc->back;
	alloc->back	    = calloc(sizeof *alloc, 1);

	if (alloc->back == NULL)
		return -1;

	alloc->back->buf  = *str;
	alloc->back->back = temp;

	return sz;
}

static inline void *sc_move(struct sc_mem *alloc, void **buf)
{
	struct sc_mem *temp = alloc->back;
	alloc->back	    = calloc(sizeof *alloc, 1);

	if (alloc->back == NULL)
		return NULL;

	alloc->back->buf  = *buf;
	alloc->back->back = temp;
	*buf		  = NULL;

	return alloc->back->buf;
}

static inline void sc_free(struct sc_mem alloc)
{
	void	      *buf  = alloc.buf;
	struct sc_mem *next = alloc.back;

	for (;;) {
		free(buf);

		if (next == NULL)
			break;

		void *temp = next;

		buf	   = next->buf;
		next	   = next->back;

		free(temp);
	}
}

#endif // JAYVM_MEM_H
