#include "jary/defs.h"

#include "jary/common.h"
#include "jary/error.h"
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

__use_result static int regenerate(struct jy_defs *tbl, uint32_t capacity)
{
	uint32_t moff1 = sizeof(*(tbl->keys)) * capacity;
	uint32_t moff2 = moff1 + sizeof(*(tbl->vals)) * capacity;
	uint32_t moff3 = moff2 + sizeof(*(tbl->types)) * capacity;

	char *mem      = jry_alloc(moff3);

	if (mem == NULL)
		return ERROR_NOMEM;

	void *keys	      = mem;
	void *vals	      = mem + moff1;
	void *types	      = mem + moff2;

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

		int status	    = jry_add_def(&newtbl, key, val, type);

		if (status != ERROR_SUCCESS) {
			jry_free_def(newtbl);
			return status;
		}
	}

	jry_free_def(*tbl);

	*tbl = newtbl;

	return ERROR_SUCCESS;
}

bool jry_find_def(const struct jy_defs *tbl, const char *key, uint32_t *id)
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

int jry_add_def(struct jy_defs *tbl,
		const char     *key,
		union jy_value	value,
		enum jy_ktype	type)
{
	int status = ERROR_SUCCESS;

	if (tbl->capacity == 0)
		status = regenerate(tbl, 8);
	else if (tbl->size + 1 >= tbl->capacity)
		status = regenerate(tbl, tbl->capacity << 1);

	if (status != ERROR_SUCCESS)
		goto FINISH;

	uint32_t id;

	size_t length = strlen(key);

	find_entry(tbl, key, length, &id);

	tbl->keys[id] = jry_alloc(length + 1);
	memcpy(tbl->keys[id], key, length);
	tbl->keys[id][length]  = '\0';
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
