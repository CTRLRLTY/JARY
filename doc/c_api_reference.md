# C API Reference
This document contains all information needed to use the functions available in `JARY/include/jary.h`.

## Functions
| Return Type | Function Signature |
|--------|------|
| `int` | `jary_open(struct jary **ctx)` |
| `int` | `jary_close(struct jary *ctx)` |
| `int` | `jary_modulepath(struct jary *, const char *path)` |
| `int` | `jary_event(struct jary *ctx, const char *name, unsigned int *event)` |
| `int` | `jary_field_str(struct jary *ctx, unsigned int event, const char *field, const char *value)` |
| `int` | `jary_field_long(struct jary *ctx, unsigned int event, const char *field, long value)` |
| `int` | `jary_field_ulong(struct jary *ctx, unsigned int event, const char *field, unsigned long value)` |
| `int` | `jary_field_bool(struct jary *ctx, unsigned int event, const char *field, unsigned char value)` |
| `int` | `jary_rule_clbk(struct jary *ctx, const char *name, int (*callback)(void *, const struct jyOutput *), void *data)` |
| `int` | `jary_compile_file(struct jary *ctx, const char *path, char **errmsg)` |
| `int` | `jary_compile(struct jary *ctx, unsigned int size, const char *source, char **errmsg)` |
| `int` | `jary_execute(struct jary *ctx)` |
| `void` | `jary_output_len(const struct jyOutput *output, unsigned int *length)` |
| `int` | `jary_output_str(const struct jyOutput *output, unsigned int index, char **value)` |
| `int` | `jary_output_long(const struct jyOutput *output, unsigned int index, long *value)` |
| `int` | `jary_output_ulong(const struct jyOutput *output, unsigned int index, unsigned long *value)` |
| `int` | `jary_output_bool(const struct jyOutput *output, unsigned int index, unsigned char *truthy)` |
| `const char*` | `jary_errmsg(struct jary *ctx)` |
| `void` | `jary_free(void *ptr)` |

## Function Descriptions

### `int jary_open`
```c
int jary_open(struct jary **ctx)
```
This function initialize the jary `ctx` and **must be called** before running other functions that expects a jary `ctx`. 

#### Return value
- `JARY_OK` everything went well, and no error.
- `JARY_ERR_OOM` Out of memory
- `JARY_ERROR` Generic error, check `jary_errmsg()` for detail, and then close it `jary_close()`.

#### Example usage:
```c
struct jary *jary;

switch (jary_open(&jary)) {
case JARY_OK:
	break;
case JARY_ERROR:
	jary_close(jary);
	printf("%s"\n, jary_errmsg(jary));
	break;
case JARY_ERR_OOM:
	// handle this
	break;
};
```

## `int jary_close`
```c
int jary_close(struct jary *ctx)
```
This function must be called to properly close an open `ctx`.

#### Return value
- `JARY_OK` everything went well, and no error.
- `JARY_ERR_SQLITE3` closing the sqlite3 db went wrong. Check `jary_errmsg()` to see the `sqlite3_errmsg()`. 

## `int jary_modulepath`
```c
int `jary_modulepath(struct jary *ctx, const char *path)
```
This function is used to set the directory for `import` resolution. When using this function, you must always suffix the `path` with the `/` character. Not doing so will lead to undefined behaviour.

Internally, whenever an `import <name>` statement is being compiled, it will perform the following concatenation:
```
solib_path = <path> + "lib" + <name>
```
this will then be passed as an argument to `dlopen(solib_path)`.

#### Return value
- `JARY_OK` everything went well, and no error.
- `JARY_ERR_OOM` out of memory.

#### Example usage
```c
switch (jary_modulepath(jary, "./myproject/")) {
case JARY_OK:
	break;
case JARY_ERR_OOM:
	printf("%s", jary_errmsg(jary));
	break;
}
```

## `int jary_event`
```c
int jary_event(struct jary *ctx, const char *name, unsigned int *event)
```

Create a new event of type `name` and push it into the event queue. The `name` argument is used to determine the type of event corresponding to the identified ingress declaration. The `event` argument will store the `id` of the of the newly queued event to be used in the `jary_field_*` function variant.

#### Return value
- `JARY_OK` everything went well, and no error.
- `JARY_ERR_NOTEXIST` there's no such event identified by `name`.
- `JARY_ERR_OOM` out of memory.

#### Example usage
```c
unsigned int event;

switch (jary_event(jary, "user", &event)) {
case JARY_OK:
	break;
case JARY_ERR_NOTEXIST:
case JARY_ERR_OOM
	printf("%s", jary_errmsg(jary));
	break;
}
```
## `int jary_field_*`
```c
int jary_field_str(struct jary *ctx, unsigned int event, const char *field, const char *value)
int jary_field_long(struct jary *ctx, unsigned int event, const char *field, long value)
int jary_field_ulong(struct jary *ctx, unsigned int event, const char *field, unsigned long value)
int jary_field_bool(struct jary *ctx, unsigned int event, const char *field, unsigned char value)
```
Define the field value for an event that has been queued. The `field` argument is used to determine the field to be defined with the `value` argument.

#### Return value
- `JARY_OK` everything went well, and no error.
- `JARY_NOT_EXIST` either the `event` id or the identified `field` does not exist, check `jary_errmsg`.
- `JARY_ERR_MISMATCH` invalid `value` type for the identified `field`.
- `JARY_ERR_OOM` out of memory.

#### Example usage
```c
unsigned int event;

// assuming "user" ingress is declared
jary_event(jary, "user", &event);

// set user.name = "John Doe"
switch(jary_field_str(jary, event, "name", "John Doe")) {
case JARY_OK:
	break;
case JARY_ERR_NOTEXIST:
	printf("%s", jary_errmsg(jary));
	break;
case JARY_ERR_MISTMATCH:
	//handle this
case JARY_ERR_OOM:
	//handle this
}
```

