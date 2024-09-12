#ifndef JAYVM_VALUE_H
#define JAYVM_VALUE_H

struct jy_obj_str;
struct jy_obj_func;
struct jy_defs;

typedef unsigned long jy_val_t;

static inline jy_val_t jry_long2v(long num)
{
	union {
		jy_val_t      bits;
		unsigned long num;
	} v;

	v.num = num;

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

static inline jy_val_t jry_str2v(struct jy_obj_str *s)
{
	return (jy_val_t) s;
}

static inline jy_val_t jry_func2v(struct jy_obj_func *f)
{
	return (jy_val_t) f;
}

static inline jy_val_t jry_def2v(struct jy_defs *def)
{
	return (jy_val_t) def;
}

static inline struct jy_obj_str *jry_v2str(jy_val_t val)
{
	return (struct jy_obj_str *) val;
}

static inline struct jy_obj_func *jry_v2func(jy_val_t val)
{
	return (struct jy_obj_func *) val;
}

static inline struct jy_defs *jry_v2def(jy_val_t val)
{
	return (struct jy_defs *) val;
}

#endif // JAYVM_VALUE_H
