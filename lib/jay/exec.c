#include "exec.h"

#include "compiler.h"

#include "jary/error.h"
#include "jary/memory.h"
#include "jary/object.h"

#include <string.h>

#define JY_K_OBJECT                                                            \
JY_K_STR:                                                                      \
	case JY_K_EVENT:                                                       \
	case JY_K_MODULE:                                                      \
	case JY_K_FUNC

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

static inline union jy_value pop(struct stack *s)
{
	jry_assert(s->size > 0);

	return s->values[--s->size];
}

static inline int interpret(const union jy_value    *vals,
			    const enum jy_ktype	    *types,
			    const void		    *obj,
			    struct stack	    *stack,
			    union flag8		    *flag,
			    const uint8_t	   **code,
			    struct jy_obj_allocator *rbuf)
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

		switch (types[*arg.u8]) {
		case JY_K_OBJECT:
			v.obj = memory_fetch(obj, v.ofs);
			break;
		default:
			break;
		}

		if (push(stack, v))
			goto FATAL;

		pc += 2;
		break;
	}
	case JY_OP_PUSH16: {
		union jy_value v = vals[*arg.u16];

		switch (types[*arg.u16]) {
		case JY_K_OBJECT:
			v.obj = memory_fetch(obj, v.ofs);
			break;
		default:
			break;
		}

		if (push(stack, v))
			goto FATAL;

		pc += 3;
		break;
	}
	case JY_OP_EVENT: {
		uint16_t field	    = arg.u16[0];
		uint16_t k_id	    = arg.u16[1];

		long		ofs = vals[k_id].ofs;
		struct jy_defs *ev  = memory_fetch(obj, ofs);

		if (push(stack, ev->vals[field]))
			goto FATAL;

		pc += 5;
		break;
	}
	case JY_OP_CALL: {
		uint16_t k_id		= arg.u16[1];
		uint8_t	 paramsz	= arg.u8[2];

		long		    ofs = vals[k_id].ofs;
		struct jy_obj_func *ofn = memory_fetch(obj, ofs);

		union jy_value args[paramsz];

		for (size_t i = 0; i < paramsz; ++i)
			args[i] = pop(stack);

		union jy_value retval;
		ofn->func(paramsz, args, &retval);

		switch (ofn->return_type) {
		case JY_K_LONG:
		case JY_K_STR:
		case JY_K_BOOL:
			push(stack, retval);
			break;
		default:
			break;
		}

		pc += 4;

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
		result.str	 = alloc_obj(allocsz, allocsz, rbuf);

		if (result.str == NULL)
			goto FATAL;

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
FATAL:
	return 1;
}

int jry_exec(const union jy_value *vals,
	     const enum jy_ktype  *types,
	     const void		  *obj,
	     const uint8_t	  *codes,
	     uint32_t		   codesz)
{
	struct stack   stack	     = { .values = NULL };
	union flag8    flag	     = { .flag = 0 };
	int	       res	     = 0;
	const uint8_t *pc	     = codes;
	const uint8_t *end	     = &codes[codesz - 1];

	// runtime object buffer
	struct jy_obj_allocator rbuf = { .buf = NULL };

	for (; end - pc > 0;) {
		res = interpret(vals, types, obj, &stack, &flag, &pc, &rbuf);

		if (res != 0)
			break;
	}

	jry_free(rbuf.buf);
	jry_free(stack.values);

	return res;
}
