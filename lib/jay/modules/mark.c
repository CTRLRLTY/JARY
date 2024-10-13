#include "jary/modules.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const float table_load_factor = 0.75;
static const int   tombstone	     = -1;

struct table {
	char **keys;
	int   *num;
	int    capacity;
	int    size;
} marked;

static uint64_t keyhash(const char *str, size_t length)
{
	uint64_t hash = 14695981039346656037u;

	for (uint32_t i = 0; i < length; ++i) {
		hash ^= (uint8_t) str[i];
		hash *= 1099511628211u;
	}

	return hash;
}

static bool find_entry(const struct table *tbl,
		       const char	  *key,
		       size_t		   keysz,
		       int		  *id)
{
	if (tbl->capacity == 0) {
		*id = 0;
		return false;
	}

	uint64_t hash	  = keyhash(key, keysz);
	int	 capacity = tbl->capacity;
	int	 index	  = hash & (capacity - 1);

	// linear probe
	for (;; index = (index + 1) & (capacity - 1)) {
		const char *k = tbl->keys[index];
		int	    v = tbl->num[index];

		if (k == NULL) {
			if (v == tombstone)
				continue;

			*id = index;
			return false;
		}

		if (*k == *key && strcmp(k, key) == 0) {
			*id = index;
			return true;
		}
	}
}

static inline void free_table(struct table tbl)
{
	for (int i = 0; i < tbl.capacity; ++i)
		free(tbl.keys[i]);

	free(tbl.keys);
	free(tbl.num);
}

static int regenerate_table(struct table *tbl, int capacity)
{
	struct table newtbl = {
		.capacity = capacity,
		.size	  = tbl->size,
	};

	newtbl.keys = malloc(sizeof(*newtbl.keys) * capacity);

	if (newtbl.keys == NULL)
		goto OUT_OF_MEMORY;

	newtbl.num = malloc(sizeof(*newtbl.num) * capacity);

	if (newtbl.num == NULL)
		goto OUT_OF_MEMORY;

	memset(newtbl.keys, 0, sizeof(*newtbl.keys) * capacity);
	memset(newtbl.num, 0, sizeof(*newtbl.num) * capacity);

	for (int i = 0; i < tbl->capacity; ++i) {
		if (tbl->keys[i] == NULL)
			continue;

		newtbl.keys[i] = tbl->keys[i];
		newtbl.num[i]  = tbl->num[i];
	}

	free_table(*tbl);
	*tbl = newtbl;

	return 0;

OUT_OF_MEMORY:
	free_table(newtbl);
	return -1;
}

static int mark(int argc, union jy_value *argv, union jy_value *result)
{
	(void) argc;
	(void) result;

	struct jy_obj_str *str	   = argv[0].str;
	struct table *restrict tbl = &marked;
	const char *key		   = str->cstr;
	size_t	    keysz	   = str->size;

	if (tbl->size + 1 > tbl->capacity * table_load_factor) {
		int oldcap = tbl->capacity;
		int newcap = (oldcap) < 8 ? 8 : (oldcap) * 2;

		if (regenerate_table(tbl, newcap) != 0)
			return -1;
	}

	int id;

	if (!find_entry(tbl, key, keysz, &id)) {
		// + 1 to include '\0'
		tbl->keys[id] = malloc(keysz + 1);

		if (tbl->keys[id] == NULL)
			return -1;

		memcpy(tbl->keys[id], key, keysz);
		tbl->keys[id][keysz]  = '\0';
		tbl->size	     += 1;
	}

	tbl->num[id] += 1;

	return 0;
}

static int unmark(int argc, union jy_value *argv, union jy_value *result)
{
	(void) argc;
	(void) result;

	struct jy_obj_str *str	   = argv[0].str;
	struct table *restrict tbl = &marked;

	int id;

	if (!find_entry(tbl, str->cstr, str->size, &id))
		return 0;

	free(tbl->keys[id]);

	tbl->keys[id]  = NULL;
	tbl->num[id]   = tombstone;
	tbl->size     -= 1;

	return 0;
}

static int count(int argc, union jy_value *argv, union jy_value *result)
{
	(void) argc;

	struct jy_obj_str *str	   = argv[0].str;
	struct table *restrict tbl = &marked;

	int id;

	if (find_entry(tbl, str->cstr, str->size, &id))
		result->i64 = tbl->num[id];
	else
		result->i64 = 0;

	return 0;
}

int module_load(struct jy_module *ctx)
{
	enum jy_ktype params = JY_K_STR;

	def_func(ctx, "mark", JY_K_TARGET, 1, &params, (jy_funcptr_t) mark);
	def_func(ctx, "unmark", JY_K_TARGET, 1, &params, (jy_funcptr_t) unmark);
	def_func(ctx, "count", JY_K_LONG, 1, &params, (jy_funcptr_t) count);

	return 0;
}

int module_unload(struct jy_module *ctx)
{
	del_func(ctx, "mark");
	del_func(ctx, "unmark");
	del_func(ctx, "count");

	free_table(marked);

	return 0;
}
