#ifndef JAYVM_MEM_H
#define JAYVM_MEM_H

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

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

typedef void (*free_t)(void *);

struct sb_mem {
	void	*buf;
	uint32_t capacity;
	uint32_t size;
};

// shitty memory allocator...
struct sc_mem {
	void	      *buf;
	struct sc_mem *back;
	void (*expire)(void *);
};

struct su_mem {
	struct su_w {
		struct su_mem *self;
		int	       ord;
		int	       size;
		char	       ptr[];
	} **buf;

	int size;
};

static inline void ifree(void **ptr)
{
	free(*ptr);
}

void *sb_alloc(struct sb_mem *sb, uint32_t nmemb);
void *sb_reserve(struct sb_mem *sb, uint32_t nmemb);
void  sb_free(struct sb_mem *sb);

void *su_alloc(struct su_mem *alloc, void *scptr, uint32_t nmemb);
void  su_free(struct su_mem *alloc);

void *sc_alloc(struct sc_mem *alloc, uint32_t nmemb);
void *sc_allocf(struct sc_mem *alloc, uint32_t nmemb, free_t expire);
int   sc_strfmt(struct sc_mem *alloc, char **str, const char *fmt, ...);
int   sc_reap(struct sc_mem *alloc, void *buf, free_t expire);
void  sc_free(struct sc_mem *alloc);

#endif // JAYVM_MEM_H
