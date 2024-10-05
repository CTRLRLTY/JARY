#ifndef JAYVM_TYPES_H
#define JAYVM_TYPES_H

enum jy_ktype {
	JY_K_UNKNOWN = 0,
	JY_K_RULE,
	JY_K_INGRESS,

	JY_K_MODULE,

	JY_K_FUNC,
	JY_K_TARGET,

	JY_K_EVENT,
	JY_K_LONG,
	JY_K_STR,
	JY_K_BOOL,

	JY_K_HANDLE,
};

// generic view for all qualified Jary values
union jy_value {
	void		   *obj;
	void		   *handle;
	struct jy_obj_func *func;
	struct jy_obj_str  *str;
	struct jy_defs	   *def;
	struct jy_defs	   *module;
	long		    i64;
	long		    ofs;
};

typedef int (*jy_funcptr_t)(int, union jy_value *, union jy_value *);

#ifndef __cplusplus
// Just for clarity
_Static_assert(sizeof(union jy_value) == 8, "Values must be 8 bytes");
#endif // __cplusplus

#endif // JAYVM_TYPES_H
