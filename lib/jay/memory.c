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

#include "jary/memory.h"

// SHITTY MEMORY

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int sb_flag = SB_NOGROW | SB_NOREGEN;

void *sb_alloc(struct sb_mem *sb, int flag, uint32_t nmemb)
{
	if (sb_reserve(sb, flag, nmemb) == NULL)
		goto OUT_OF_MEMORY;

	memset((char *) sb->buf + sb->size, 0, nmemb);
	sb->size += nmemb;

	return sb->buf;
OUT_OF_MEMORY:
	return NULL;
}

void *sb_append(struct sb_mem *sb, int flag, uint32_t nmemb)
{
	if (sb_reserve(sb, flag, nmemb) == NULL)
		goto OUT_OF_MEMORY;

	char *mem  = sb->buf;
	mem	  += sb->size;
	memset(mem, 0, nmemb);

	sb->size += nmemb;

	return mem;
OUT_OF_MEMORY:
	return NULL;
}

void *sb_reserve(struct sb_mem *sb, int flag, uint32_t nmemb)
{
	const uint32_t growth = 1
			      + 1 * (sb_flag & SB_NOGROW & ~(flag & SB_NOGROW));
	const int nextsz = sb->size + nmemb;

	if (nextsz > sb->capacity) {
		if (!(sb_flag & SB_NOREGEN & ~(flag & SB_NOREGEN)))
			goto OUT_OF_MEMORY;

		uint32_t newsz = (sb->capacity + nmemb) * growth;
		void	*mem   = realloc(sb->buf, newsz);

		if (mem == NULL)
			goto OUT_OF_MEMORY;

		sb->capacity = newsz;
		sb->buf	     = mem;
	}

	return sb->buf;
OUT_OF_MEMORY:
	return NULL;
}

void sb_free(struct sb_mem *sb)
{
	free(sb->buf);
}

void *sc_alloc(struct sc_mem *alloc, uint32_t nmemb)
{
	void	      *block = calloc(nmemb, 1);
	struct sc_mem *back  = NULL;

	if (block == NULL)
		goto OUT_OF_MEMORY;

	back = malloc(sizeof *alloc);

	if (back == NULL)
		goto OUT_OF_MEMORY;

	back->buf    = block;
	back->expire = free;
	back->back   = alloc->back;
	alloc->back  = back;

	return block;

OUT_OF_MEMORY:
	free(block);
	free(alloc->back);
	return NULL;
}

void *su_alloc(struct su_mem *alloc, void *scptr, uint32_t nmemb)
{
	struct su_w *buf = NULL;

	if (scptr == NULL) {
		buf = calloc(sizeof *buf + nmemb, 1);

		if (buf == NULL)
			goto FAIL;

		alloc->size += 1;
		buf->self    = alloc;
		buf->ord     = alloc->size - 1;
		buf->size    = nmemb;

		alloc->buf = realloc(alloc->buf,
				     sizeof(struct su_w *) * alloc->size);

		alloc->buf[alloc->size - 1] = buf;
	} else {
		struct su_w *temp = (struct su_w *) scptr - 1;

		assert(temp->self == alloc);

		buf = calloc(sizeof *buf + nmemb + temp->size, 1);

		if (buf == NULL)
			goto FAIL;

		memcpy(buf, temp, sizeof(*buf) + temp->size);
		buf->size	      += nmemb;
		alloc->buf[temp->ord]  = buf;

		free(temp);
	}

	return buf->ptr;
FAIL:
	return NULL;
}

void *sc_allocf(struct sc_mem *alloc, uint32_t nmemb, free_t expire)
{
	void *block = calloc(nmemb, 1);

	if (block == NULL)
		return NULL;

	struct sc_mem *temp = alloc->back;
	alloc->back	    = calloc(sizeof *alloc, 1);

	if (alloc->back == NULL)
		return NULL;

	alloc->back->buf    = block;
	alloc->back->expire = expire;
	alloc->back->back   = temp;

	return block;
}

int sc_strfmt(struct sc_mem *alloc, char **str, const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	int sz = vasprintf(str, fmt, arg);
	va_end(arg);

	if (*str == NULL)
		goto OUT_OF_MEMORY;

	struct sc_mem *oldback = alloc->back;
	struct sc_mem *back    = (struct sc_mem *) calloc(sizeof *alloc, 1);

	if (back == NULL)
		goto OUT_OF_MEMORY;

	alloc->back	    = back;
	alloc->back->buf    = *str;
	alloc->back->expire = free;
	alloc->back->back   = oldback;
	return sz;

OUT_OF_MEMORY:
	free(*str);
	return 0;
}

int sc_reap(struct sc_mem *alloc, void *buf, free_t expire)
{
	struct sc_mem *temp = alloc->back;
	alloc->back	    = calloc(sizeof *alloc, 1);

	if (alloc->back == NULL)
		goto OUT_OF_MEMORY;

	alloc->back->buf    = buf;
	alloc->back->expire = expire;
	alloc->back->back   = temp;

	return 0;

OUT_OF_MEMORY:
	return -1;
}

void sc_free(struct sc_mem *alloc)
{
	void	      *buf     = alloc->buf;
	struct sc_mem *next    = alloc->back;
	void (*expire)(void *) = alloc->expire;

	for (;;) {
		if (expire)
			expire(buf);

		if (next == NULL)
			break;

		void *temp = next;

		buf    = next->buf;
		expire = next->expire;
		next   = next->back;

		free(temp);
	}
}

void su_free(struct su_mem *alloc)
{
	for (int i = 0; i < alloc->size; ++i) {
		struct su_w *buf = alloc->buf[i];

		free(buf);
	}

	free(alloc->buf);
}
