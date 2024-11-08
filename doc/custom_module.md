# JARY Dynamic Modules
This document contains all needed information to write a compliant JARY dynamic library. 

Jary use a hot swappable plugin system approach for its modules. A module is defined as a `.so` object which will be loaded at runtime using `dlopen()`. Every module needs to implement two core functions:

```c
int module_load(struct jy_module *ctx, const char **errmsg);
int module_unload(struct jy_module *ctx, const char **errmsg);
```

These function signature can be seen within the `JARY/include/jary/modules.h` header file, which also contain other APIs to be used within the module.

## Module API


### `int jary_def`
```c
int jay_def_func(struct jy_module    *ctx,
		 const char	     *key,
		 enum jy_ktype	      return_type,
		 uint8_t	      param_size,
		 const enum jy_ktype *param_types,
		 int (*func)(struct jy_state *,
			     int,
			     union jy_value *,
			     union jy_value *));
```

