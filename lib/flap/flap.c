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

#include "flap.h"

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

		arg   = &(*arglist)[hsz];
		*hsz += 1;

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

