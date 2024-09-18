#include "jary/defs.h"

#include "jary/common.h"
#include "jary/error.h"
#include "jary/memory.h"

#include <stdint.h>
#include <string.h>

static uint64_t keyhash(uint64_t seed, const char *str, uint32_t length)
{
	uint64_t hash = 14695981039346656037u ^ seed;

	for (uint32_t i = 0; i < length; ++i) {
		hash ^= (uint8_t) str[i];
		hash *= 1099511628211u;
	}

	return hash;
}

static inline bool find_entry(struct jy_defs *tbl, uint64_t hash, uint32_t *id)
{
	int	      capacity = tbl->capacity;
	uint32_t      index    = hash & (capacity - 1);
	enum jy_ktype type     = tbl->types[index];

	*id		       = index;

	return type != JY_K_UNKNOWN;
}

__use_result static int regenerate(struct jy_defs *tbl,
				   uint64_t	   seed,
				   uint32_t	   capacity)
{
	uint32_t moff1 = sizeof(*(tbl->keys)) * capacity;
	uint32_t moff2 = moff1 + sizeof(*(tbl->keysz)) * capacity;
	uint32_t moff3 = moff2 + sizeof(*(tbl->vals)) * capacity;
	uint32_t moff4 = moff3 + sizeof(*(tbl->types)) * capacity;

	char *mem      = jry_alloc(moff4);

	if (mem == NULL)
		return ERROR_NOMEM;

	void *keys	      = mem;
	void *keysz	      = mem + moff1;
	void *vals	      = mem + moff2;
	void *types	      = mem + moff3;

	struct jy_defs newtbl = { .keys	    = keys,
				  .keysz    = keysz,
				  .vals	    = vals,
				  .types    = types,
				  .seed	    = seed,
				  .capacity = capacity,
				  .size	    = 0 };

	memset(mem, 0, moff4);

	for (unsigned int i = 0; i < tbl->size; ++i) {
		enum jy_ktype type = tbl->types[i];

		if (type != JY_K_UNKNOWN)
			continue;

		char	*key   = tbl->keys[i];
		uint32_t keysz = tbl->keysz[i];
		jy_val_t val   = tbl->vals[i];

		int status     = jry_add_def(&newtbl, key, keysz, val, type);

		if (status != ERROR_SUCCESS) {
			jry_free_def(newtbl);
			return status;
		}
	}

	jry_free_def(*tbl);

	*tbl = newtbl;

	return ERROR_SUCCESS;
}

bool jry_find_def(struct jy_defs *tbl,
		  const char	 *key,
		  uint32_t	  length,
		  uint32_t	 *id)
{
	if (tbl->size == 0)
		return false;

	uint32_t entryid;
	uint64_t hash  = keyhash(tbl->seed, key, length);
	bool	 found = find_entry(tbl, hash, &entryid);

	if (!found)
		return false;

	const char *ekey   = tbl->keys[entryid];
	uint32_t    ekeysz = tbl->keysz[entryid];

	if (id != NULL)
		*id = entryid;

	if (*key != *ekey)
		return false;

	if (length != ekeysz)
		return false;

	return memcmp(key, ekey, ekeysz) == 0;
}

int jry_add_def(struct jy_defs *tbl,
		const char     *key,
		uint32_t	length,
		jy_val_t	value,
		enum jy_ktype	type)
{
	int status = ERROR_SUCCESS;

	if (tbl->capacity == 0)
		status = regenerate(tbl, tbl->seed, 8);
	else if (tbl->size + 1 >= tbl->capacity)
		status = regenerate(tbl, tbl->seed, tbl->capacity << 1);

	if (status != ERROR_SUCCESS)
		goto FINISH;

	uint32_t id;

	for (;;) {
		uint64_t hash  = keyhash(tbl->seed, key, length);

		bool collision = find_entry(tbl, hash, &id);

		if (!collision)
			break;

		status = regenerate(tbl, tbl->seed + 1, tbl->capacity);

		if (status != ERROR_SUCCESS)
			goto FINISH;
	}

	tbl->keys[id] = jry_alloc(length + 1);
	memcpy(tbl->keys[id], key, length);
	tbl->keys[id][length]  = '\0';
	tbl->keysz[id]	       = length;
	tbl->vals[id]	       = value;
	tbl->types[id]	       = type;
	tbl->size	      += 1;

FINISH:
	return status;
}

void jry_free_def(struct jy_defs tbl)
{
	for (unsigned int i = 0; i < tbl.capacity; ++i)
		jry_free(tbl.keys[i]);

	jry_free(tbl.keys);
}
