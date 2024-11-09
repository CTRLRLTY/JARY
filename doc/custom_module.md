# JARY Dynamic Modules
This document contains all needed information to write a compliant JARY dynamic library. 

Jary use a hot swappable plugin system approach for its modules. A module is defined as a `.so` object which will be loaded at runtime using `dlopen()`. Every module needs to implement theese two core functions:

```c
int module_load(struct jy_module *ctx, const char **errmsg);
int module_unload(struct jy_module *ctx, const char **errmsg);
```

These function signature can be seen within the `JARY/include/jary/modules.h` header file, which also contain other APIs to be used within the module.

## Module API

### `int jay_def_func`
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
Define a function to be exported by the module. This function is supposed to only to be used within the `module_load` function.
- `ctx`: The module where the function is defined.
- `key`: The name of the function
- `return type`: The function's return value type
- `param_size`: The number of arguments the function takes.
- `param_types`: Types for each function argument.
- `func`: The function implementation to be executed.

**Return value**
- `JAY_OK` nothing went wrong
- `JAY_ERR_OOM` out of memory

The `func` implementation accepts the following arguments in order:
- `state` opaque pointer to the current runtime state
- `argc` length of the argument array
- `argv` argument value array
- `retval` return value of the function

The func can also return various codes that affect the VM's behavior:
- `JAY_OK` default behaviour

### `int jay_del_func`
```c
int jay_del_func(struct jy_module *ctx, const char *key);
```
Every defined function have to deleted on `module_unload` using this function. The `key` has to correspond to exact one used to define it.

- `ctx`: The module where the function is defined.
- `key`: The name of the function

**Return value**
- `JAY_OK` nothing went wrong
- `JAY_ERR_MISMATCH` invalid function `key`

> Not calling this function will lead to memory leak.

# Creating Custom Modules 
JARY modules are not exclusive to C; they just need to implement the required functions and expose them as dynamic symbols. In the following examples, we’ll create a module for logging values to the standard output. The initial directory structure will look like this:

```sh
log/
    example.jary
```

where `example.jary` contains the following:

```
import log

// define the expected structure of the user event
ingress user {
  field:
    name string
    activity string
}

rule auth_brute_force {
  match:
    $user.activity exact "failed login"
    $user within 10s

condition:
    1 == 1

action:
    log.info("hello world!")
}
```

Then we can just go into that directory and start from there.

## C example
To get started creating a jary module in C, we must to download the `JARY/include/jary/modules.h` file and store it in any directory, here i will place it in `log/`.

The `log.c` file contains the following code

```c
#include "modules.h"

#include <stdio.h>

static int info(struct jy_state *state,
		int		 argc,
		union jy_value	*argv,
		union jy_value	*retval)
{
	(void) state;
	(void) argc;
	(void) retval;

	struct jy_str *str = argv[0].str;

	printf("%s\n", str->cstr);

	return JAY_OK;
}

// this will be called when the module gets loaded
// PARAM1 ctx    : the current module ctx, which in this case is log
// PARAM2 errmsg : an error message that'll be used by JARY on certain JAY_INT_* codes  
int module_load(struct jy_module *ctx, const char **errmsg)
{
	// log.info only accept 1 argument
	enum jy_ktype params = JY_K_STR;

	// define log.info action function
	switch (jay_def_func(ctx, "info", JY_K_ACTION, 1, &params, info)) {
	case JAY_OK:
		*errmsg = "nothing went wrong";
		break;
	case JAY_ERR_OOM:
		// crash the module
		*errmsg = jay_errmsg(ctx);
		return JAY_INT_CRASH;
	}

	return JAY_OK;
}

// This will be called when the module get unloaded
// PARAM1 ctx    : the current module ctx, which in this case is log
// PARAM2 errmsg : an error message that'll be used by JARY on certain JAY_INT_* codes  
int module_unload(struct jy_module *ctx, const char **errmsg)
{
	// delete log.info
	jay_del_func(ctx, "info");

	*errmsg = "nothing went wrong";

	return JAY_OK;
}
```

To compile the code and create the `.so` object:
```sh
gcc  -fPIC -c ./log.c -o log.o
gcc -shared -oliblog.so log.o
```

Now, to test the module, create a simple `C` program named `example.c` to execute the Jary runtime

```c
#include <jary.h>
#include <stdio.h>

int main(int argc, const char **argv)
{
	const char *filepath = argv[1];

	char *compile_errmsg;

	struct jary *jary;

	if (jary_open(&jary) != JARY_OK)
		goto PANIC;

	switch (jary_compile_file(jary, filepath, &compile_errmsg)) {
	case JARY_OK:
		break;
	case JARY_ERR_COMPILE:
		goto COMPILE_FAIL;
	default:
		goto PANIC;
	}

	unsigned int event;

	if (jary_event(jary, "user", &event) != JARY_OK)
		goto PANIC;

	// set user.name = "root"
	if (jary_field_str(jary, event, "name", "root") != JARY_OK)
		goto PANIC;

	// set user.activity = "failed login"
	if (jary_field_str(jary, event, "activity", "failed login") != JARY_OK)
		goto PANIC;

	// Execute all rules
	if (jary_execute(jary) != JARY_OK)
		goto PANIC;

	goto FINISH;

PANIC:
	fprintf(stderr, "%s\n", jary_errmsg(jary));
	goto FINISH;

COMPILE_FAIL:
	fprintf(stderr, "%s\n", compile_errmsg);
	jary_free(compile_errmsg);

FINISH:
	jary_close(jary);

	return 0;
}
```

To compile it:
```sh
gcc ./example.c -ljary -o example
```

Finally, to test the module, run:

```sh
LD_LIBRARY_PATH=. ./example example.jary 
```

If everything is set up correctly, you should see the following output:
```
hello world!
```





