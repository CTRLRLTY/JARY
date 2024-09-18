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
};

typedef unsigned long jy_val_t;

#ifndef __cplusplus
// Just for clarity
_Static_assert(sizeof(jy_val_t) == 8, "Values must be 8 bytes");
#endif // __cplusplus

#endif // JAYVM_TYPES_H
