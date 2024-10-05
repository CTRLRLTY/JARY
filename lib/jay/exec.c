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

static inline int interpret(const union jy_value *vals,
			    const enum jy_ktype	 *types,
			    const void		 *obj,
			    struct stack	 *stack,
			    union flag8		 *flag,
			    const uint8_t	**code)
{
	const uint8_t *pc     = *code;
	enum jy_opcode opcode = *pc;

	union {
		const uint8_t  *bytes;
		const uint8_t  *u8;
		const int16_t  *i16;
		const uint16_t *u16;
		const uint32_t *u32;
	} arg = { .bytes = pc + 1 };

	switch (opcode) {
	case JY_OP_PUSH8: {
		union {
			union jy_value val;
			void	      *ptr;
		} v = { .val = vals[*arg.u8] };

		switch (types[*arg.u8]) {
		case JY_K_OBJECT:
			v.ptr = memory_fetch(obj, v.val.ofs);
			break;
		default:
			break;
		}

		if (push(stack, v.val))
			goto FATAL;

		pc += 2;
		break;
	}
	case JY_OP_PUSH16: {
		union {
			union jy_value val;
			void	      *ptr;
		} v = { .val = vals[*arg.u16] };

		switch (types[*arg.u8]) {
		case JY_K_OBJECT:
			v.ptr = memory_fetch(obj, v.val.ofs);
			break;
		default:
			break;
		}

		if (push(stack, v.val))
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

		flag->bits.b8	      = *v1->str == *v2->str &&
				memcmp(v1->str, v2->str, v1->size) == 0;

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
	struct stack   stack = { .values = NULL };
	union flag8    flag  = { .flag = 0 };
	int	       res   = 0;
	const uint8_t *pc    = codes;
	const uint8_t *end   = &codes[codesz - 1];

	for (; end - pc > 0;) {
		res = interpret(vals, types, obj, &stack, &flag, &pc);

		if (res != 0)
			break;
	}

	jry_free(stack.values);

	return res;
}
