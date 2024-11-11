/*
BSD 3-Clause License

Copyright (c) 2024. Muhammad Raznan. All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "exec.h"

#include "compiler.h"
#include "storage.h"

#include "jary/defs.h"
#include "jary/memory.h"
#include "jary/types.h"

#include <assert.h>
#include <errno.h>
#include <sqlite3.h>
#include <stdbool.h>
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

struct match_data {
	struct jy_defs	     *names;
	const uint8_t	     *codes;
	struct sc_mem	     *alloc;
	const union jy_value *vals;
	struct jy_state *restrict state;
};

// TODO: This is so ugly.... but im in a hurry.
struct runtime {
	// runtime memory scratch
	struct sc_mem buf;
	struct stack  stack;
	struct sqlite3 *restrict db;
	const union jy_value *vals;
	struct jy_defs	     *names;
	const uint8_t	    **pc;
	const uint8_t	     *fcodes;
	union flag8	      flag;
};

static inline int interpret(struct runtime *ctx,
			    struct jy_state *restrict state);

static inline void free_runtime(struct runtime *restrict ctx)
{
	sb_free(&ctx->stack.m);
	sc_free(&ctx->buf);
}

static inline bool push(struct stack *s, union jy_value value)
{
	uint32_t idx = s->m.size / sizeof(value);
	void	*mem = sb_alloc(&s->m, 0, sizeof(value));

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

// Convert C string to value
static inline int value_from_cstr(struct sc_mem	 *alloc,
				  union jy_value *value,
				  enum jy_ktype	  type,
				  const char	 *str)
{
	switch (type) {
	case JY_K_STR: {
		struct jy_str *ostr;
		uint32_t       sz = 0;

		if (str != NULL)
			sz = strlen(str);

		// +1 to include '\0'
		ostr = sc_alloc(alloc, sizeof(*ostr) + sz + 1);

		if (ostr == NULL)
			goto OUT_OF_MEMORY;

		ostr->size = sz;
		memcpy(ostr->cstr, str, sz);
		value->str = ostr;
		break;
	}

	case JY_K_BOOL:
	case JY_K_ULONG:
	case JY_K_LONG: {
		errno = 0;

		if (str != NULL)
			value->i64 = strtol(str, NULL, 10);
		else
			value->i64 = 0;

		if (errno == EINVAL)
			goto INV_VALUE;
		break;
	}
	default:
		goto INV_VALUE;
	}

	return 0;

OUT_OF_MEMORY:
	return 1;

INV_VALUE:
	return 2;
}

static inline int match_clbk(struct match_data *data,
			     int		colsz,
			     char	      **values,
			     char	      **columns)
{
	int		      ret   = SQLITE_OK;
	struct jy_defs	     *names = data->names;
	struct sc_mem	     *alloc = data->alloc;
	const uint8_t	     *codes = data->codes;
	const union jy_value *vals  = data->vals;
	struct runtime	      ctx   = {
			 .names = names,
			 .vals	= vals,
			 .pc	= &codes,
	};

	for (int i = 0; i < colsz; ++i) {
		union jy_value	view  = { .handle = NULL };
		struct jy_defs *event = NULL;
		size_t		len   = strlen(columns[i]) + 1;

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

		if (def_get(names, key, &view, NULL))
			goto PANIC;

		event = view.def;

		if (event == NULL)
			goto PANIC;

		enum jy_ktype type;

		if (def_get(event, member, &view, &type))
			goto PANIC;

		// TODO: Handle error message
		if (value_from_cstr(alloc, &view, type, values[i]))
			goto PANIC;

		if (def_set(event, member, view, type))
			goto PANIC;
	}

	struct jy_state *state = data->state;
	for (; **ctx.pc != JY_OP_END;)
		switch (interpret(&ctx, state)) {
		case 0:
			continue;
		default:
			goto PANIC;
		};

	goto FINISH;

PANIC:
	ret = SQLITE_ABORT;

FINISH:
	free_runtime(&ctx);
	return ret;
}

static inline int interpret(struct runtime *ctx,
			    struct jy_state *restrict state)
{
	assert(ctx->vals != NULL);
	assert(ctx->pc != NULL);
	assert(*ctx->pc != NULL);
	assert(ctx->names != NULL);

	int		      ret    = 0;
	struct sqlite3	     *db     = ctx->db;
	const union jy_value *vals   = ctx->vals;
	const uint8_t	    **code   = ctx->pc;
	const uint8_t	     *fcodes = ctx->fcodes;
	struct jy_defs	     *names  = ctx->names;

	struct stack  *stack  = &ctx->stack;
	union flag8   *flag   = &ctx->flag;
	struct sc_mem *rbuf   = &ctx->buf;
	struct sc_mem *sbuf   = state ? state->buf : rbuf;
	struct sc_mem  bump   = { .buf = NULL };
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
	case JY_OP_END:
		// shouldnt come here
		assert(opcode != JY_OP_END);
		break;
	case JY_OP_SETBF8:
		flag->bits.b8  = pop(stack).i64;
		pc	      += 1;
		break;

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

		struct jy_func *func = pop(stack).func;

		union jy_value retval;

		func->func(state, paramsz, args, &retval);

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
	case JY_OP_LOAD: {
		struct jy_desc	d     = pop(stack).dscptr;
		struct jy_defs *event = vals[d.name].def;
		union jy_value	v     = event->vals[d.member];

		if (push(stack, v))
			goto OUT_OF_MEMORY;

		pc += 1;
		break;
	}

	case JY_OP_OUTPUT: {
		uint64_t       length = pop(stack).u64;
		union jy_value values[length];

		for (uint64_t i = 0; i < length; ++i)
			values[i] = pop(stack);

		for (uint64_t i = length; i > 0; --i) {
			union jy_value v = values[i - 1];
			state->out	 = sb_add(state->outm, 0, sizeof(v));

			if (state->out == NULL)
				goto OUT_OF_MEMORY;

			state->out[state->outsz]  = v;
			state->outsz		 += 1;
		}

		pc += 1;
		break;
	}
	case JY_OP_BETWEEN: {
		long		max   = pop(stack).i64;
		long		min   = pop(stack).i64;
		struct jy_desc	d     = pop(stack).dscptr;
		struct jy_defs *event = vals[d.name].def;

		struct QMbetween *Q = sc_alloc(rbuf, sizeof *Q);
		uint32_t	  namefield;

		if (!def_find(event, "__name__", &namefield))
			goto INVARIANT;

		Q->type	  = QM_BETWEEN;
		Q->max	  = max;
		Q->min	  = min;
		Q->table  = event->vals[namefield].str->cstr;
		Q->column = event->keys[d.member];

		union jy_value view = { .handle = Q };

		if (push(stack, view))
			goto OUT_OF_MEMORY;

		pc += 1;
		break;
	}
	case JY_OP_WITHIN: {
		struct jy_time_ofs timeofs = pop(stack).timeofs;
		struct jy_desc	   d	   = pop(stack).dscptr;
		struct jy_defs	  *names   = vals[d.name].def;

		struct QMwithin *Q = sc_alloc(rbuf, sizeof *Q);

		if (Q == NULL)
			goto OUT_OF_MEMORY;

		Q->type	   = QM_WITHIN;
		Q->table   = names->keys[d.member];
		Q->column  = "__arrival__";
		Q->timeofs = timeofs;

		union jy_value view = { .handle = Q };

		if (push(stack, view))
			goto OUT_OF_MEMORY;

		pc += 1;
		break;
	}
	case JY_OP_JOIN: {
		struct jy_desc	d2     = pop(stack).dscptr;
		struct jy_desc	d1     = pop(stack).dscptr;
		struct jy_defs *event2 = vals[d2.name].def;
		struct jy_defs *event1 = vals[d1.name].def;

		uint32_t field2;
		uint32_t field1;

		if (!def_find(event1, "__name__", &field1))
			goto INVARIANT;

		if (!def_find(event2, "__name__", &field2))
			goto INVARIANT;

		struct QMjoin *Q = sc_alloc(rbuf, sizeof *Q);

		if (Q == NULL)
			goto OUT_OF_MEMORY;

		Q->type	     = QM_JOIN;
		Q->tbl_left  = event2->vals[field2].str->cstr;
		Q->col_left  = event2->keys[d2.member];
		Q->tbl_right = event1->vals[field1].str->cstr;
		Q->col_right = event1->keys[d1.member];

		union jy_value view = { .handle = Q };

		if (push(stack, view))
			goto OUT_OF_MEMORY;

		pc += 1;
		break;
	}
	case JY_OP_REGEX: {
		struct jy_str  *regexstr = pop(stack).str;
		struct jy_desc	d	 = pop(stack).dscptr;
		struct jy_defs *event	 = vals[d.name].def;
		uint32_t	field;

		struct QMbinary *Q = sc_alloc(rbuf, sizeof *Q);

		if (!def_find(event, "__name__", &field))
			goto INVARIANT;

		if (Q == NULL)
			goto OUT_OF_MEMORY;

		Q->type		  = QM_BINARY;
		Q->table	  = event->vals[field].str->cstr;
		Q->column	  = event->keys[d.member];
		Q->value.type	  = QME_REGEXP;
		Q->value.as.regex = regexstr->cstr;

		union jy_value view = { .handle = Q };

		if (push(stack, view))
			goto OUT_OF_MEMORY;

		pc += 1;

		break;
	}
	case JY_OP_EQUAL: {
		union jy_value	right  = pop(stack);
		struct jy_desc	dscptr = pop(stack).dscptr;
		struct jy_defs *event  = vals[dscptr.name].def;

		uint32_t field;

		if (!def_find(event, "__name__", &field))
			goto INVARIANT;

		struct QMbinary *Q = sc_alloc(rbuf, sizeof *Q);

		if (Q == NULL)
			goto OUT_OF_MEMORY;

		Q->type	  = QM_BINARY;
		Q->table  = event->vals[field].str->cstr;
		Q->column = event->keys[dscptr.member];

		switch (event->types[dscptr.member]) {
		case JY_K_STR:
			Q->value.type	 = QME_CSTR;
			Q->value.as.cstr = right.str->cstr;
			break;
		case JY_K_LONG:
			Q->value.type	= QME_LONG;
			Q->value.as.i64 = right.i64;
			break;
		default:
			goto QUERY_FAILED;
		}

		union jy_value sql = { .handle = Q };

		if (push(stack, sql))
			goto OUT_OF_MEMORY;

		pc += 1;
		break;
	}
	case JY_OP_QUERY: {
		unsigned long  ofs   = pop(stack).ofs;
		const uint8_t *chunk = fcodes + ofs;
		long	       qlen  = pop(stack).i64;
		struct QMbase *qs[qlen];

		for (int i = 0; i < qlen; ++i)
			qs[i] = pop(stack).handle;

		struct Qmatch Q = {
			.qlen  = qlen,
			.qlist = qs,
			.names = names,
		};

		q_clbk *callback = (q_clbk *) match_clbk;

		struct match_data data = {
			.names = names,
			.alloc = &bump,
			.codes = chunk,
			.vals  = vals,
			.state = state,
		};

		switch (q_match(db, NULL, callback, &data, Q)) {
		case 1:
			goto OUT_OF_MEMORY;
		case 2:
			goto QUERY_FAILED;
		}

		pc += 1;
		break;
	}
	case JY_OP_CMPSTR: {
		struct jy_str *v2 = pop(stack).str;
		struct jy_str *v1 = pop(stack).str;
		const char    *s2 = v2->cstr;
		const char    *s1 = v1->cstr;
		size_t	       z2 = v2->size;
		size_t	       z1 = v1->size;

		flag->bits.b8  = z1 == z2 && memcmp(s1, s2, z1) == 0;
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

		struct jy_str *v2 = pop(stack).str;
		struct jy_str *v1 = pop(stack).str;

		size_t strsz = v2->size + v1->size;
		char   buf[strsz + 1];

		strcpy(buf, v1->cstr);
		strcat(buf, v2->cstr);

		uint32_t allocsz = sizeof(struct jy_str) + strsz + 1;
		result.str	 = sc_alloc(sbuf, allocsz);

		if (result.str == NULL)
			goto OUT_OF_MEMORY;

		result.str->size = strsz;
		memcpy(result.str->cstr, buf, strsz);

		push(stack, result);

		pc += 1;
		break;
	}
	default:
		goto INVARIANT;
	}

	goto FINISH;

OUT_OF_MEMORY:
	ret = 1;
	goto FINISH;
QUERY_FAILED:
	ret = 2;
	goto FINISH;

INVARIANT:
	ret = 3;

FINISH:
	sc_free(&bump);

	// PC is not moving...
	assert(pc != *code);

	*code = pc;
	return ret;
}

int jry_exec(struct sqlite3	 *db,
	     const struct jy_jay *jay,
	     const uint8_t	 *codes,
	     struct jy_state	 *state)
{
	int		      ret  = 0;
	const union jy_value *vals = jay->vals;
	const uint8_t	     *pc   = codes;

	struct runtime ctx = {
		.db	= db,
		.names	= jay->names,
		.vals	= vals,
		.pc	= &pc,
		.fcodes = jay->fcodes,
	};

	for (; *pc != JY_OP_END;) {
		switch (interpret(&ctx, state)) {
		case 0:
			continue;
		case 1:
			goto OUT_OF_MEMORY;
		case 2:
			goto QUERY_FAILED;
		}
	}

	goto FINISH;

OUT_OF_MEMORY:
	ret = 1;
	goto FINISH;

QUERY_FAILED:
	ret = 2;

FINISH:
	free_runtime(&ctx);
	return ret;
}
