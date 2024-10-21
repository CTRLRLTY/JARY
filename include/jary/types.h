#ifndef JAYVM_TYPES_H
#define JAYVM_TYPES_H

#include <stdint.h>

enum jy_opcode {
	JY_OP_PUSH8,
	JY_OP_PUSH16,

	JY_OP_JOIN,
	JY_OP_EXACT,
	JY_OP_JMPT,
	JY_OP_JMPF,
	JY_OP_CALL,

	JY_OP_QUERY,

	JY_OP_NOT,
	JY_OP_CMPSTR,
	JY_OP_CMPFIELD,
	JY_OP_CMP,
	JY_OP_LT,
	JY_OP_GT,

	JY_OP_ADD,
	JY_OP_CONCAT,
	JY_OP_SUB,
	JY_OP_MUL,
	JY_OP_DIV,

	JY_OP_END
};

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

struct jy_jay {
	// global names
	struct jy_defs *names;
	// code chunk array
	uint8_t	       *codes;
	// constant table
	union jy_value *vals;
	enum jy_ktype  *types;
	uint32_t	codesz;
	uint16_t	valsz;
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
