#ifndef JAYVM_TYPES_H
#define JAYVM_TYPES_H

#include <stdint.h>

enum jy_ktype {
	JY_K_UNKNOWN = 0,
	JY_K_RULE,
	JY_K_INGRESS,

	JY_K_MODULE,
	JY_K_DESCRIPTOR,

	JY_K_FUNC,
	JY_K_TARGET,
	JY_K_MATCH,

	JY_K_EVENT,
	JY_K_LONG_FIELD,
	JY_K_STR_FIELD,
	JY_K_BOOL_FIELD,

	JY_K_LONG,
	JY_K_STR,
	JY_K_BOOL,

	JY_K_HANDLE,
};

struct jy_descriptor {
	uint32_t name;
	uint32_t member;
};

struct jy_field {
	int		size;
	enum jy_ktype	type;
	union jy_value *as;
};

// generic view for all qualified Jary values
union jy_value {
	void		    *obj;
	void		    *handle;
	struct jy_obj_func  *func;
	struct jy_obj_str   *str;
	char		    *cstr;
	struct jy_defs	    *def;
	struct jy_defs	    *module;
	struct jy_field	    *field;
	struct jy_descriptor dscptr;
	long		     i64;
	long		     ofs;
};

typedef int (*jy_funcptr_t)(int, union jy_value *, union jy_value *);

struct jy_obj_str {
	// size does not include '\0'
	uint32_t size;
	// null terminated string
	char	 cstr[];
};

struct jy_obj_func {
	enum jy_ktype return_type;
	uint8_t	      param_size;
	jy_funcptr_t  func;
	enum jy_ktype param_types[];
};

#ifndef __cplusplus
// Just for clarity
_Static_assert(sizeof(union jy_value) == 8, "Values must be 8 bytes");
#endif // __cplusplus

#endif // JAYVM_TYPES_H
