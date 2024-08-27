#include "jary/value.h"

#include "jary/memory.h"

#include <string.h>

jy_val_t jry_long_val(int64_t num)
{
	union {
		jy_val_t bits;
		int64_t	 num;
	} v;

	v.num = num;

	return v.bits;
}

jy_val_t jry_str_val(const char *str, size_t length)
{
	strhash_t	   hash = jry_strhash(str, length);
	struct jy_obj_str *s	= jry_alloc(sizeof(*s));

	s->str			= strndup(str, length);
	s->size			= length;
	s->hash			= hash;

	union {
		jy_val_t	   bits;
		struct jy_obj_str *obj;
	} v;

	v.obj = s;

	return v.bits;
}

int64_t jry_val_long(jy_val_t val)
{
	union {
		jy_val_t bits;
		int64_t	 num;
	} v;

	v.bits = val;

	return v.num;
}

struct jy_obj_str *jry_val_str(jy_val_t val)
{
	return (struct jy_obj_str *) val;
}