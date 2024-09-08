#include "flap.h"

#include "fnv.h"

#include "jary/memory.h"

#include <stdint.h>

enum atype {
	ARG_SHORT,
	ARG_LONG,
	ARG_ARGUMENT,
};

static struct meta {
} optmeta;

static struct parsed {
	fhash_t	    *hash;
	const char **arg;
	enum atype  *types;
	// hash size
	size_t	     size;
} opts;

static inline void arg2type(const char *arg, enum atype *type)
{
	char c = arg[0];

	switch (c) {
	case '-':
		if (arg[1] == '-')
			*type = ARG_LONG;
		else
			*type = ARG_SHORT;

		break;
	default:
		*type = ARG_ARGUMENT;
	}
}

static void parse(enum atype *types, const char **args, size_t length)
{
	fhash_t	    **hlist    = &opts.hash;
	char ***const arglist  = &opts.arg;
	enum atype  **typelist = &opts.types;
	size_t	     *hsz      = &opts.size;

	for (size_t i = 0; i < length; ++i) {
		enum atype start = types[0];
		enum atype next	 = types[1];

		const char **arg;
		fhash_t	     shash;

		shash = fnv_hash(start);

		jry_mem_push(*hlist, *hsz, shash);
		jry_mem_push(**typelist, *hsz, *types);
		jry_mem_push(**arglist, *hsz, NULL);

		types += 1;
		args  += 1;

		arg    = &(*arglist)[hsz];
		*hsz  += 1;

		if (next == ARG_ARGUMENT) {
			*arg   = *args;
			types += 1;
			args  += 1;
		}
	}
}

void flap_args(const char **args, size_t length)
{
	enum atype types[length];

	for (size_t i = 0; i < length; ++i) {
		enum atype t;
		arg2type(args[i], &t);
		types = t;
	}

	const char **stdarg = args + 1;
	size_t	     alen   = length - 1;
	parse(stdarg, alen);
}

void flap_opt(char	  opt,
	      const char *alias,
	      size_t	  length,
	      const char *desc,
	      size_t	  dscsz)
{
}