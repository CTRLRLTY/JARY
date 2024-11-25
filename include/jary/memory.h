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

/**************************
 *
 *   A SHITTY MEMORY!
 *
 *************************/

#ifndef JAYVM_MEM_H
#define JAYVM_MEM_H

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#define jry_alloc(__size)	   malloc((__size))
#define jry_realloc(__ptr, __size) realloc((__ptr), (__size))
#define jry_free(__ptr)		   free((__ptr))

// calculate if sz is a power of 2
#define jry_mem_full(__sz)	   (((__sz) & ((__sz) - 1)) == 0)
// grow memory from sz
#define jry_mem_grow(__sz, __ptr)                                              \
	jry_realloc((__ptr), sizeof(*(__ptr)) * ((__sz) << 1))

// push data to array
#define jry_mem_push(__ptr, __sz, __data)                                      \
	do {                                                                   \
		if (jry_mem_full((__sz) + 1))                                  \
			(__ptr) = jry_mem_grow(((__sz) + 1), (__ptr));         \
                                                                               \
		if ((__ptr) != NULL)                                           \
			(__ptr)[(__sz)] = (__data);                            \
	} while (0)

typedef void (*free_t)(void *);

struct sb_mem {
	void *buf;
	int   capacity;
	int   size;
};

// shitty memory allocator...
struct sc_mem {
	void	      *buf;
	struct sc_mem *back;
	void (*expire)(void *);
};

struct su_mem {
	struct su_w {
		struct su_mem *self;
		int	       ord;
		int	       size;
		char	       ptr[];
	} **buf;

	int size;
};

static inline void ifree(void **ptr)
{
	free(*ptr);
}

#define SB_NOGROW  0x1
#define SB_NOREGEN 0x2

void *sb_alloc(struct sb_mem *sb, int flag, uint32_t nmemb);
void *sb_append(struct sb_mem *sb, int flag, uint32_t nmemb);
void *sb_add(struct sb_mem *sb, int flag, uint32_t nmemb);
void *sb_reserve(struct sb_mem *sb, int flag, uint32_t nmemb);
void  sb_free(struct sb_mem *sb);

void *su_alloc(struct su_mem *alloc, void *scptr, uint32_t nmemb);
void  su_free(struct su_mem *alloc);

void *sc_alloc(struct sc_mem *alloc, uint32_t nmemb);
void *sc_allocf(struct sc_mem *alloc, uint32_t nmemb, free_t expire);
int   sc_strfmt(struct sc_mem *alloc, char **str, const char *fmt, ...);
int   sc_reap(struct sc_mem *alloc, void *buf, free_t expire);
void  sc_free(struct sc_mem *alloc);

static inline struct sb_mem *sc_linbuf(struct sc_mem *alloc)
{
	void *ptr = sc_alloc(alloc, sizeof(struct sb_mem));

	if (sc_reap(alloc, ptr, (free_t) sb_free))
		return 0;

	return (struct sb_mem *) ptr;
}

#endif // JAYVM_MEM_H
