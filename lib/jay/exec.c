#include "exec.h"

#include "compiler.h"

#include "jary/error.h"
#include "jary/memory.h"
#include "jary/object.h"

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

struct query {
	const char *table;
	const char *column;

	union {
		const char   *str;
		struct query *Q;
	} as;

	enum {
		Q_EXACT,
		Q_JOIN,
	} type;
};

struct stack {
	union jy_value *values;
	uint32_t	size;
	uint32_t	capacity;
};

static inline bool push(struct stack *s, union jy_value value)
{
	const int growth = 10;

	if (s->size + 1 >= s->capacity) {
		int   newcap = (s->capacity + growth);
		void *mem = jry_realloc(s->values, sizeof(*s->values) * newcap);

		if (mem == NULL)
			return true;

		s->capacity = newcap;
		s->values   = mem;
	}

	s->values[s->size]  = value;
	s->size		   += 1;

	return false;
}

static inline bool exists(const struct query *qs, int len, const struct query Q)
{
	for (int i = 0; i < len; ++i) {
		if (strcmp(qs[i].table, Q.table))
			continue;

		if (strcmp(qs[i].column, Q.column) == 0)
			return true;
	}

	return false;
}

static inline void querystr(char **sql, const struct query *qs, int qlen)
{
	struct sc_mem buf = { .buf = NULL };

	struct query joins[qlen];
	struct query exacts[qlen];
	struct query uniq[qlen * 2];

	int uniqsz  = 0;
	int joinsz  = 0;
	int exactsz = 0;

	memset(joins, 0, sizeof(joins));
	memset(exacts, 0, sizeof(exacts));
	memset(uniq, 0, sizeof(uniq));

	for (int i = 0; i < qlen; ++i) {
		struct query Q = qs[i];

		if (!exists(uniq, uniqsz, Q)) {
			uniq[i]	 = Q;
			uniqsz	+= 1;
		}

		switch (Q.type) {
		case Q_EXACT:
			exacts[i]  = Q;
			exactsz	  += 1;
			break;
		case Q_JOIN:
			joins[i]  = Q;
			joinsz	 += 1;
			break;
		}
	}

	sc_strfmt(&buf, sql, "SELECT");

	if (*sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < uniqsz - 1; ++i) {
		const char *t = uniq[i].table;
		const char *c = uniq[i].column;

		sc_strfmt(&buf, sql, "%s %s.%s,", *sql, t, c);

		if (*sql == NULL)
			goto OUT_OF_MEMORY;
	}

	struct query Q = uniq[uniqsz - 1];

	sc_strfmt(&buf, sql, "%s %s.%s", *sql, Q.table, Q.column);

	if (*sql == NULL)
		goto OUT_OF_MEMORY;

	sc_strfmt(&buf, sql, "%s FROM", *sql);

	if (*sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < uniqsz - 1; ++i) {
		const char *t = uniq[i].table;

		sc_strfmt(&buf, sql, "%s %s,", *sql, t);

		if (*sql == NULL)
			goto OUT_OF_MEMORY;
	}

	sc_strfmt(&buf, sql, "%s %s", *sql, Q.table, Q.column);

	if (*sql == NULL)
		goto OUT_OF_MEMORY;

	sc_strfmt(&buf, sql, "%s WHERE", *sql);

	if (*sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < joinsz; ++i) {
		struct query Q	= joins[i];
		const char  *lt = Q.table;
		const char  *lc = Q.column;
		const char  *rt = Q.as.Q->table;
		const char  *rc = Q.as.Q->column;

		sc_strfmt(&buf, sql, "%s %s.%s = %s.%s,", *sql, lt, lc, rt, rc);

		if (*sql == NULL)
			goto OUT_OF_MEMORY;
	}

	for (int i = 0; i < exactsz; ++i) {
		struct query Q	= exacts[i];
		const char  *lt = Q.table;
		const char  *lc = Q.column;
		const char  *r	= Q.as.str;

		sc_strfmt(&buf, sql, "%s %s.%s = %s,", *sql, lt, lc, r);

		if (*sql == NULL)
			goto OUT_OF_MEMORY;
	}

	int len		= strlen(*sql);
	(*sql)[len - 1] = ';';
	buf.back->buf	= NULL;
	sc_free(buf);
	return;
OUT_OF_MEMORY:
	*sql = NULL;
	sc_free(buf);
}

static inline union jy_value pop(struct stack *s)
{
	jry_assert(s->size > 0);

	return s->values[--s->size];
}

static inline int interpret(const union jy_value *vals,
			    struct stack	 *stack,
			    union flag8		 *flag,
			    const uint8_t	**code,
			    struct sc_mem	 *rbuf)
{
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
	case JY_OP_EXACT: {
		struct jy_obj_str   *str    = pop(stack).str;
		struct jy_descriptor dscptr = pop(stack).dscptr;

		uint32_t	id;
		struct jy_defs *event = vals[dscptr.name].def;

		jry_assert(jry_find_def(event, "__name__", &id));

		struct query *Q	   = sc_alloc(rbuf, sizeof *Q);
		Q->table	   = event->vals[id].str->cstr;
		Q->column	   = event->keys[dscptr.member];
		Q->type		   = Q_EXACT;
		Q->as.str	   = str->cstr;
		union jy_value sql = { .handle = Q };

		if (push(stack, sql))
			goto OUT_OF_MEMORY;

		pc += 1;
		break;
	}
	case JY_OP_QUERY: {
		long	     qlen = pop(stack).i64;
		struct query qs[qlen];

		for (int i = 0; i < qlen; ++i) {
			struct query *Q = pop(stack).handle;
			qs[i]		= *Q;
		}

		char *sql;
		querystr(&sql, qs, qlen);

		if (sql == NULL)
			goto OUT_OF_MEMORY;

		if (sc_move(rbuf, (void **) &sql) == NULL)
			goto OUT_OF_MEMORY;

		pc += 1;
		break;
	}
	case JY_OP_CMPSTR: {
		struct jy_obj_str *v2 = pop(stack).str;
		struct jy_obj_str *v1 = pop(stack).str;
		flag->bits.b8	      = *v1->cstr == *v2->cstr &&
				memcmp(v1->cstr, v2->cstr, v1->size) == 0;

		pc += 1;
		break;
	}
	case JY_OP_CMP: {
		long v2	       = pop(stack).i64;
		long v1	       = pop(stack).i64;
		flag->bits.b8  = v1 == v2;

		pc	      += 1;
		break;
	}
	case JY_OP_LT: {
		long v2	       = pop(stack).i64;
		long v1	       = pop(stack).i64;

		flag->bits.b8  = v1 < v2;

		pc	      += 1;
		break;
	}
	case JY_OP_GT: {
		long v2	       = pop(stack).i64;
		long v1	       = pop(stack).i64;

		flag->bits.b8  = v1 > v2;

		pc	      += 1;
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

		size_t strsz	      = v2->size + v1->size;
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
	jry_assert(pc != *code);
	*code = pc;
	return 0;

OUT_OF_MEMORY:
	return 1;
}

int jry_exec(const union jy_value *vals, const uint8_t *codes, uint32_t codesz)
{
	struct stack   stack = { .values = NULL };
	union flag8    flag  = { .flag = 0 };
	int	       res   = 0;
	const uint8_t *pc    = codes;
	const uint8_t *end   = &codes[codesz - 1];

	// runtime object buffer
	struct sc_mem rbuf   = { .buf = NULL };

	for (; end - pc > 0;) {
		res = interpret(vals, &stack, &flag, &pc, &rbuf);

		if (res != 0)
			break;
	}

	sc_free(rbuf);
	jry_free(stack.values);

	return res;
}