#include "exec.h"

#include "compiler.h"
#include "storage.h"

#include "jary/memory.h"

#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

union flag8 {
	uint8_t flag;

	struct bits {
		bool b1 : 1;
		bool b2 : 1;
		bool b3 : 1;
		bool b4 : 1;
		bool b5 : 1;
		bool b6 : 1;
		bool b7 : 1;
		bool b8 : 1;
	} bits;
};

struct stack {
	union jy_value *values;
	struct sb_mem	m;
};

struct match_cb_data {
	struct jy_defs *names;
	struct sc_mem  *alloc;
	int		colsz;
};

// TODO: This is so ugly.... but im in a hurry.
struct runtime {
	struct sqlite3	     *db;
	const union jy_value *vals;
	struct stack	     *stack;
	union flag8	     *flag;
	struct jy_defs	     *names;
	const uint8_t	    **pc;
	// runtime memory scratch
	struct sc_mem	     *buf;
};

static inline bool push(struct stack *s, union jy_value value)
{
	uint32_t idx = s->m.size / sizeof(value);
	void	*mem = sb_alloc(&s->m, sizeof(value));

	if (mem == NULL)
		return true;

	s->values      = mem;
	s->values[idx] = value;

	return false;
}

static inline union jy_value pop(struct stack *s)
{
	uint32_t idx = s->m.size / sizeof(*s->values);
	assert(idx > 0);

	s->m.size -= sizeof(*s->values);

	return s->values[--idx];
}

static inline int fill_event(struct sc_mem *restrict alloc,
			     struct jy_defs *restrict event,
			     const char *member,
			     const char *text)
{
	union jy_value f = { .field = NULL };
	union jy_value v = { .handle = NULL };

	def_get(event, member, &f, NULL);

	assert(f.field != NULL);

	switch (f.field->type) {
	case JY_K_STR: {
		size_t sz = strlen(text);
		// +1 to include '\0'
		v.str	  = sc_alloc(alloc, sizeof(*v.str) + sz + 1);

		if (v.str == NULL)
			goto OUT_OF_MEMORY;

		v.str->size = sz;
		memcpy(v.str->cstr, text, sz);
		break;
	}
	case JY_K_LONG:
		v.i64 = strtol(text, NULL, 10);
		break;
	default:
		goto INV_FIELD;
	}

	jry_mem_push(f.field->as, f.field->size, v);

	if (f.field->as == NULL)
		goto OUT_OF_MEMORY;

	f.field->size += 1;

	return 0;
INV_FIELD:
	return 1;
OUT_OF_MEMORY:
	return -1;
}

static inline bool field_cmp(struct jy_field *field, union jy_value value)
{
	bool result = false;

	for (int i = 0; i < field->size; ++i) {
		union jy_value fv = field->as[i];

		switch (field->type) {
		case JY_K_STR: {
			const char *s1 = fv.str->cstr;
			const char *s2 = value.str->cstr;
			size_t	    z1 = fv.str->size;
			size_t	    z2 = value.str->size;

			result = z1 == z2 && memcmp(s1, s2, z1) == 0;
			break;
		}
		case JY_K_LONG:
		case JY_K_BOOL:
			result = fv.i64 == value.i64;
			break;

		default:
			return false;
		}

		if (!result)
			break;
	}

	return result;
}

static inline int match_clbk(struct match_cb_data *data,
			     int		   colsz,
			     char		 **values,
			     char		 **columns)
{
	data->colsz += 1;

	struct jy_defs *def   = data->names;
	struct sc_mem  *alloc = data->alloc;

	for (int i = 0; i < colsz; ++i) {
		union jy_value event = { .def = NULL };
		size_t	       len   = strlen(columns[i]) + 1;

		char  buf[len];
		char *sep;
		char *key;
		char *member;

		mempcpy(buf, columns[i], len);
		sep = strchr(buf, '.');

		assert(sep != NULL);

		*sep   = '\0';
		key    = buf;
		member = sep + 1;

		def_get(def, key, &event, NULL);

		assert(event.def != NULL);

		switch (fill_event(alloc, event.def, member, values[i])) {
		case -1:
			goto OUT_OF_MEMORY;
		default:
			continue;
		}
	}

	return 0;

OUT_OF_MEMORY:
	return -1;
}

static inline int interpret(struct runtime ctx)
{
	struct sqlite3	     *db    = ctx.db;
	const union jy_value *vals  = ctx.vals;
	struct stack	     *stack = ctx.stack;
	union flag8	     *flag  = ctx.flag;
	const uint8_t	    **code  = ctx.pc;
	struct sc_mem	     *rbuf  = ctx.buf;
	struct jy_defs	     *names = ctx.names;

	const uint8_t *pc     = *code;
	enum jy_opcode opcode = *pc;

	union {
		const uint8_t  *bytes;
		const int16_t  *i16;
		const uint8_t  *u8;
		const uint16_t *u16;
		const uint32_t *u32;
	} arg = { .bytes = pc + 1 };

	switch (opcode) {
	case JY_OP_PUSH8: {
		union jy_value v = vals[*arg.u8];

		if (push(stack, v))
			goto OUT_OF_MEMORY;

		pc += 2;
		break;
	}
	case JY_OP_PUSH16: {
		union jy_value v = vals[*arg.u16];

		if (push(stack, v))
			goto OUT_OF_MEMORY;

		pc += 3;
		break;
	}
	case JY_OP_CALL: {
		uint8_t paramsz = arg.u8[0];

		union jy_value args[paramsz];

		for (size_t i = 0; i < paramsz; ++i)
			args[i] = pop(stack);

		struct jy_descriptor desc = pop(stack).dscptr;
		struct jy_defs	    *def  = vals[desc.name].def;
		struct jy_obj_func  *func = def->vals[desc.member].func;

		union jy_value retval;

		func->func(paramsz, args, &retval);

		switch (func->return_type) {
		case JY_K_LONG:
		case JY_K_STR:
		case JY_K_BOOL:
			push(stack, retval);
			break;
		default:
			break;
		}

		pc += 2;

		break;
	}
	case JY_OP_JMPF:
		if (!flag->bits.b8)
			pc += *arg.i16;
		else
			pc += 3;
		break;
	case JY_OP_JMPT:
		if (flag->bits.b8)
			pc += *arg.i16;
		else
			pc += 3;
		break;
	case JY_OP_NOT:
		flag->bits.b8  = !flag->bits.b8;
		pc	      += 1;
		break;
	case JY_OP_JOIN: {
		struct jy_descriptor d2	    = pop(stack).dscptr;
		struct jy_descriptor d1	    = pop(stack).dscptr;
		struct jy_defs	    *event2 = vals[d2.name].def;
		struct jy_defs	    *event1 = vals[d1.name].def;

		uint32_t field2;
		uint32_t field1;

		assert(def_find(event1, "__name__", &field1));
		assert(def_find(event2, "__name__", &field2));

		struct QMjoin *Q = sc_alloc(rbuf, sizeof *Q);

		if (Q == NULL)
			goto OUT_OF_MEMORY;

		Q->type	     = QM_JOIN;
		Q->tbl_left  = event2->vals[field2].str->cstr;
		Q->col_left  = event2->keys[d2.member];
		Q->tbl_right = event1->vals[field1].str->cstr;
		Q->col_right = event1->keys[d1.member];

		union jy_value sql = { .handle = Q };

		if (push(stack, sql))
			goto OUT_OF_MEMORY;

		pc += 1;
		break;
	}
	case JY_OP_EXACT: {
		struct jy_obj_str   *str    = pop(stack).str;
		struct jy_descriptor dscptr = pop(stack).dscptr;
		struct jy_defs	    *event  = vals[dscptr.name].def;

		uint32_t field;

		assert(def_find(event, "__name__", &field));

		struct QMexact *Q = sc_alloc(rbuf, sizeof *Q);

		if (Q == NULL)
			goto OUT_OF_MEMORY;

		Q->type		   = QM_EXACT;
		Q->table	   = event->vals[field].str->cstr;
		Q->column	   = event->keys[dscptr.member];
		Q->value	   = str->cstr;
		union jy_value sql = { .handle = Q };

		if (push(stack, sql))
			goto OUT_OF_MEMORY;

		pc += 1;
		break;
	}
	case JY_OP_QUERY: {
		long	       qlen = pop(stack).i64;
		struct QMbase *qs[qlen];

		for (int i = 0; i < qlen; ++i)
			qs[i] = pop(stack).handle;

		struct Qmatch Q	   = { .qlen = qlen, .qlist = qs };
		q_callback_t  clbk = (q_callback_t) match_clbk;

		struct match_cb_data data = {
			.names = names,
			.alloc = rbuf,
		};

		switch (q_match(db, NULL, clbk, &data, Q)) {
		case -1:
			goto OUT_OF_MEMORY;
		case -2:
			goto QUERY_FAILED;
		}

		flag->bits.b8 = data.colsz;

		pc += 1;
		break;
	}
	case JY_OP_CMPSTR: {
		struct jy_obj_str *v2 = pop(stack).str;
		struct jy_obj_str *v1 = pop(stack).str;
		const char	  *s2 = v2->cstr;
		const char	  *s1 = v1->cstr;
		size_t		   z2 = v2->size;
		size_t		   z1 = v1->size;

		flag->bits.b8  = z1 == z2 && memcmp(s1, s2, z1) == 0;
		pc	      += 1;
		break;
	}
	case JY_OP_CMPFIELD: {
		union jy_value	     v	   = pop(stack);
		struct jy_descriptor d	   = pop(stack).dscptr;
		struct jy_defs	    *event = vals[d.name].def;
		struct jy_field	    *field = event->vals[d.member].field;

		flag->bits.b8  = field_cmp(field, v);
		pc	      += 1;
		break;
	}
	case JY_OP_CMP: {
		long v2	      = pop(stack).i64;
		long v1	      = pop(stack).i64;
		flag->bits.b8 = v1 == v2;

		pc += 1;
		break;
	}
	case JY_OP_LT: {
		long v2 = pop(stack).i64;
		long v1 = pop(stack).i64;

		flag->bits.b8 = v1 < v2;

		pc += 1;
		break;
	}
	case JY_OP_GT: {
		long v2 = pop(stack).i64;
		long v1 = pop(stack).i64;

		flag->bits.b8 = v1 > v2;

		pc += 1;
		break;
	}
	case JY_OP_ADD: {
		union jy_value v2  = pop(stack);
		union jy_value v1  = pop(stack);
		v1.i64		  += v2.i64;

		push(stack, v1);

		pc += 1;
		break;
	}
	case JY_OP_SUB: {
		union jy_value v2  = pop(stack);
		union jy_value v1  = pop(stack);
		v1.i64		  -= v2.i64;

		push(stack, v1);

		pc += 1;
		break;
	}
	case JY_OP_MUL: {
		union jy_value v2  = pop(stack);
		union jy_value v1  = pop(stack);
		v1.i64		  *= v2.i64;

		push(stack, v1);

		pc += 1;
		break;
	}
	case JY_OP_DIV: {
		union jy_value v2  = pop(stack);
		union jy_value v1  = pop(stack);
		v1.i64		  /= v2.i64;

		push(stack, v1);

		pc += 1;
		break;
	}
	case JY_OP_CONCAT: {
		union jy_value result;

		struct jy_obj_str *v2 = pop(stack).str;
		struct jy_obj_str *v1 = pop(stack).str;

		size_t strsz = v2->size + v1->size;
		char   buf[strsz + 1];

		strcpy(buf, v1->cstr);
		strcat(buf, v2->cstr);

		uint32_t allocsz = sizeof(struct jy_obj_str) + strsz + 1;
		result.str	 = sc_alloc(rbuf, allocsz);

		if (result.str == NULL)
			goto OUT_OF_MEMORY;

		result.str->size = strsz;
		memcpy(result.str->cstr, buf, strsz);

		push(stack, result);

		pc += 1;
		break;
	}
	}

	// PC is not moving...
	assert(pc != *code);
	*code = pc;
	return 0;

OUT_OF_MEMORY:
	return -1;
QUERY_FAILED:
	return -2;
}

int jry_exec(struct sqlite3 *db, struct jy_exec *exec, const struct jy_jay *jay)
{
	const union jy_value *vals   = jay->vals;
	const uint8_t	     *codes  = jay->codes;
	uint32_t	      codesz = jay->codesz;
	const uint8_t	     *pc     = codes;
	const uint8_t	     *end    = &codes[codesz - 1];
	struct stack	      stack  = { .values = NULL };
	union flag8	      flag   = { .flag = 0 };
	struct sc_mem	      rbuf   = { .buf = NULL };
	struct runtime	      ctx    = { .db	= db,
					 .names = jay->names,
					 .vals	= vals,
					 .stack = &stack,
					 .flag	= &flag,
					 .pc	= &pc,
					 .buf	= &rbuf };

	sc_reap(&rbuf, &stack.m, (free_t) sb_free);

	for (; end - pc > 0;) {
		switch (interpret(ctx)) {
		case 0:
			continue;
		case -1:
			goto OUT_OF_MEMORY;
		default:
			goto EXEC_FAIL;
		}
	}

	sc_free(&rbuf);
	return 0;

OUT_OF_MEMORY:
	sc_free(&rbuf);
	return -1;

EXEC_FAIL:
	sc_free(&rbuf);
	return -2;
}
