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

### `int jary_open(struct jary **ctx)`
This function initialize the jary `ctx` and **must be called** before running other functions that expects a jary `ctx`. 

The second argument `db` can be used to provide the `ctx` with an already managed sqlite3 connection and will be used to store all of the data managed by `jary`.  

> The `db` argument should be set to `NULL` unless you know what you are doing.

#### Return value
- `JARY_OK` everything went well, and no error.
- `JARY_OOM` Out of memory
- `JARY_ERROR` Generic error, check `jary_errmsg()` for detail, and then close it `jary_close()`.

#### Example usage:
```c
struct jary *jary;

switch (jary_open(&jary, NULL)) {
case JARY_OK:
	break;
case JARY_ERROR:
	jary_close(jary);
	printf("%s"\n, jary_errmsg(jary));
	break;
case JARY_OOM:
	// handle this
	break;
};
```

## `int jary_close(struct jary *ctx)`
This function must be called to properly close an open `ctx`.

#### Return value
- `JARY_OK` everything went well, and no error.
- `JARY_SQLITE3` closing the sqlite3 db went wrong. Check `jary_errmsg()` to see the `sqlite3_errmsg()`. 

## int `jary_modulepath(struct jary *ctx, const char *path)`
This function is used to set the directory for `import` resolution. When using this function, you must always suffix the `path` with the `/` character. Not doing so will lead to undefined behaviour.

Internally, whenever an `import <name>` statement is being compiled, it will perform the following concatenation:
```
solib_path = <path> + "lib" + <name>
```
this will then be passed as an argument to `dlopen(solib_path)`.

#### Return value
- `JARY_OK` everything went well, and no error.
- `JARY_OOM` out of memory 

#### Example usage
```c
switch (jary_modulepath(jary, "./myproject/")) {
case JARY_OK:
	break;
case JARY_OOM:
	// handle this
	break;
}
```
