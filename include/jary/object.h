#ifndef JAYVM_OBJECT_H
#define JAYVM_OBJECT_H

#include "jary/types.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef jy_val_t (*jy_funcptr_t)(jy_val_t *, int);

struct jy_obj_str {
	char	*str;
	uint32_t size;
};

struct jy_object_allocator {
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

static inline void *jry_allocobj(uint16_t		     nmemb,
				 uint32_t		     grow,
				 struct jy_object_allocator *alloc)
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

static inline jy_val_t jry_long2v(long num)
{
	union {
		jy_val_t      bits;
		unsigned long num;
	} v;

	v.num = num;

	return v.bits;
}

static inline jy_val_t jry_str2v(struct jy_obj_str *s)
{
	return (jy_val_t) s;
}

static inline jy_val_t jry_func2v(struct jy_obj_func *f)
{
	return (jy_val_t) f;
}

static inline long jry_v2long(jy_val_t val)
{
	union {
		jy_val_t bits;
		long	 num;
	} v;

	v.bits = val;

	return v.num;
}

static inline struct jy_obj_str *jry_v2str(jy_val_t val)
{
	return (struct jy_obj_str *) val;
}

static inline struct jy_obj_func *jry_v2func(jy_val_t val)
{
	return (struct jy_obj_func *) val;
}

#endif // JAYVM_OBJECT_H
