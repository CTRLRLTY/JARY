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

#ifndef JAYVM_ERROR_H
#define JAYVM_ERROR_H

#include "jary/memory.h"

#include <stdint.h>
#include <stdlib.h>

struct tkn_errs {
	const char **msgs;
	uint32_t    *from;
	uint32_t    *to;
	uint32_t     size;
};

#ifndef __cplusplus
static inline void free_errs(struct tkn_errs *errs)
{
	free(errs->msgs);
	free(errs->from);
	free(errs->to);
}

static inline int tkn_error(struct tkn_errs *errs,
			    const char	    *msg,
			    uint32_t	     from,
			    uint32_t	     to)
{
	jry_mem_push(errs->msgs, errs->size, msg);
	jry_mem_push(errs->from, errs->size, from);
	jry_mem_push(errs->to, errs->size, to);

	if (errs->from == NULL)
		return -1;

	errs->size += 1;

	return 0;
}
#endif

#endif // JAYVM_ERROR_H
