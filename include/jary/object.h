#ifndef JAYVM_OBJECT_H
#define JAYVM_OBJECT_H

#include "jary/types.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct jy_obj_str {
	// null terminated string
	char	*cstr;
	// size does not include '\0'
	uint32_t size;
};

struct jy_obj_allocator {
	void	*buf;
	uint32_t size;
	uint32_t capacity;
};

struct jy_obj_func {
	enum jy_ktype  return_type;
	uint8_t	       param_size;
	enum jy_ktype *param_types;
	jy_funcptr_t   func;
};

static inline void *alloc_obj(uint16_t		       nmemb,
			      uint32_t		       grow,
			      struct jy_obj_allocator *alloc)
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

#endif // JAYVM_OBJECT_H
