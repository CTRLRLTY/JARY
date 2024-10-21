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

#include "jary/defs.h"

#include "jary/common.h"
#include "jary/memory.h"

#include <stdint.h>
#include <string.h>

static uint64_t keyhash(const char *str, uint32_t length)
{
	uint64_t hash = 14695981039346656037u;

	for (uint32_t i = 0; i < length; ++i) {
		hash ^= (uint8_t) str[i];
		hash *= 1099511628211u;
	}

	return hash;
}

static inline bool find_entry(const struct jy_defs *tbl,
			      const char	   *key,
			      size_t		    keysz,
			      uint32_t		   *id)
{
	uint64_t hash	  = keyhash(key, keysz);
	int	 capacity = tbl->capacity;
	uint32_t index	  = hash & (capacity - 1);

	for (;; index = (index + 1) & (capacity - 1)) {
		const char *ekey = tbl->keys[index];

		if (ekey == NULL) {
			*id = index;
			return false;
		}

		if (*key == *ekey && strcmp(ekey, key) == 0) {
			*id = index;
			return true;
		}
	}
}

static __use_result int regenerate(struct jy_defs *tbl, uint32_t capacity)
{
	uint32_t moff1 = sizeof(*(tbl->keys)) * capacity;
	uint32_t moff2 = moff1 + sizeof(*(tbl->vals)) * capacity;
	uint32_t moff3 = moff2 + sizeof(*(tbl->types)) * capacity;

	char *mem = jry_alloc(moff3);

	if (mem == NULL)
		goto OUT_OF_MEMORY;

	void *keys  = mem;
	void *vals  = mem + moff1;
	void *types = mem + moff2;

	struct jy_defs newtbl = { .keys	    = keys,
				  .vals	    = vals,
				  .types    = types,
				  .capacity = capacity,
				  .size	    = 0 };

	memset(mem, 0, moff3);

	for (unsigned int i = 0; i < tbl->capacity; ++i) {
		const char *ekey = tbl->keys[i];

		if (ekey == NULL)
			continue;

		enum jy_ktype  type = tbl->types[i];
		char	      *key  = tbl->keys[i];
		union jy_value val  = tbl->vals[i];

		if (def_add(&newtbl, key, val, type)) {
			def_free(&newtbl);
			goto OUT_OF_MEMORY;
		}
	}

	def_free(tbl);

	*tbl = newtbl;

	return 0;

OUT_OF_MEMORY:
	return -1;
}

bool def_find(const struct jy_defs *tbl, const char *key, uint32_t *id)
{
	if (tbl->size == 0)
		return false;

	uint32_t entryid;
	size_t	 keysz = strlen(key);
	bool	 found = find_entry(tbl, key, keysz, &entryid);

	if (!found)
		return false;

	const char *ekey = tbl->keys[entryid];

	if (id != NULL)
		*id = entryid;

	if (*key != *ekey)
		return false;

	return *key == *ekey && strcmp(key, ekey) == 0;
}

int def_set(struct jy_defs *tbl,
	    const char	   *key,
	    union jy_value  value,
	    enum jy_ktype   type)
{
	uint32_t id = 0;

	if (!def_find(tbl, key, &id)) {
		if (def_add(tbl, key, value, type) == -1)
			goto OUT_OF_MEMORY;
	} else {
		tbl->vals[id]  = value;
		tbl->types[id] = type;
	}

	return 0;
OUT_OF_MEMORY:
	return -1;
}

int def_get(struct jy_defs *tbl,
	    const char	   *key,
	    union jy_value *value,
	    enum jy_ktype  *type)
{
	uint32_t id = 0;

	if (!def_find(tbl, key, &id))
		goto NOT_FOUND;

	if (value)
		*value = tbl->vals[id];
	if (type)
		*type = tbl->types[id];

	return 0;
NOT_FOUND:
	return 1;
}

int def_add(struct jy_defs *tbl,
	    const char	   *key,
	    union jy_value  value,
	    enum jy_ktype   type)
{
	if (tbl->capacity == 0) {
		if (regenerate(tbl, 8))
			goto OUT_OF_MEMORY;
	} else if (tbl->size + 1 >= tbl->capacity) {
		if (regenerate(tbl, tbl->capacity << 1))
			goto OUT_OF_MEMORY;
	}

	uint32_t id;

	size_t length = strlen(key);

	// TODO: return error if readding the same definition
	find_entry(tbl, key, length, &id);

	tbl->keys[id] = jry_alloc(length + 1);
	memcpy(tbl->keys[id], key, length);
	tbl->keys[id][length]  = '\0';
	tbl->vals[id]	       = value;
	tbl->types[id]	       = type;
	tbl->size	      += 1;

	return 0;

OUT_OF_MEMORY:
	return -1;
}

void def_free(struct jy_defs *tbl)
{
	for (unsigned int i = 0; i < tbl->capacity; ++i)
		jry_free(tbl->keys[i]);

	jry_free(tbl->keys);
}

void def_clear(struct jy_defs *tbl)
{
	def_free(tbl);

	struct jy_defs newtbl = { .keys = NULL };
	*tbl		      = newtbl;
}
