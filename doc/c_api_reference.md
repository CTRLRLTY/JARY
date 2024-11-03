# C API Reference
This document contains all information needed to use the functions available in `JARY/include/jary.h`.

## Functions
| Return Type | Function Signature |
|--------|------|
| `int` | `jary_open(struct jary **ctx, struct sqlite3 *)` |
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
| `int` | `jary_output_len(const struct jyOutput *output, unsigned int *length)` |
| `int` | `jary_output_str(const struct jyOutput *output, unsigned int index, char **value)` |
| `int` | `jary_output_long(const struct jyOutput *output, unsigned int index, long *value)` |
| `int` | `jary_output_ulong(const struct jyOutput *output, unsigned int index, unsigned long *value)` |
| `int` | `jary_output_bool(const struct jyOutput *output, unsigned int index, unsigned char *truthy)` |
| `const char*` | `jary_errmsg(struct jary *ctx)` |
| `void` | `jary_free(void *ptr)` |

## Function Descriptions
