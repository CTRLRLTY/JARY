#ifndef JAYVM_OBJECT_H
#define JAYVM_OBJECT_H

#include "jary/types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef jy_val_t (*jy_funcptr_t)(int, jy_val_t *, int);

struct jy_obj_str {
	char  *str;
	size_t size;
};

struct jy_obj_func {
	enum jy_ktype  return_type;
	bool	       internal;
	uint8_t	       param_sz;
	enum jy_ktype *param_types;
	jy_funcptr_t   func;
};

// This object is inlined. Dont allocate on heap.
struct jy_obj_event {
	unsigned short event;
	unsigned short name;
	char	       __padding__[4];
};

_Static_assert(sizeof(struct jy_obj_event) == 8, "Event object must 8 bytes");

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

static inline jy_val_t jry_event2v(struct jy_obj_event ev)
{
	union {
		jy_val_t	    bits;
		struct jy_obj_event ev;
	} v;

	v.ev = ev;

	return v.bits;
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

static inline struct jy_obj_event jry_v2event(jy_val_t val)
{
	union {
		jy_val_t	    bits;
		struct jy_obj_event ev;
	} v;

	v.bits = val;

	return v.ev;
}

#endif // JAYVM_OBJECT_H
