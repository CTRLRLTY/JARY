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

#include "jary/jary.h"

#include "ast.h"
#include "compiler.h"
#include "error.h"
#include "exec.h"
#include "parser.h"
#include "token.h"

#include "jary/defs.h"
#include "jary/memory.h"

#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

struct exec {
	const struct jy_tkns  *tkns;
	const struct jy_asts  *asts;
	const struct jy_jay   *jay;
	const struct tkn_errs *errs;
};

struct jyOutput {
	unsigned int	size;
	union jy_value *values;
};

struct jary {
	struct sc_mem	sc;
	struct sb_mem	sb;
	char	       *mdir;
	const char     *errmsg;
	struct exec    *code;
	struct sqlite3 *db;
	uint8_t	       *ev_colsz;
	const char    **ev_tables;
	const char   ***ev_cols;
	const char   ***ev_vals;
	int (**r_clbks)(void *, const struct jyOutput *);
	uint16_t *r_clbk_ords;
	void	**r_clbk_datas;
	uint32_t  ev_sz;
	uint16_t  r_clbk_sz;
};

static inline int create_event_table(struct sqlite3	  *db,
				     const char		  *name,
				     const struct jy_defs *event)
{
	int	      ret = JARY_OK;
	struct sc_mem m	  = { .buf = NULL };

	char *sql;
	int   sqlsz = 0;

	sc_strfmt(&m, &sql, "CREATE TABLE IF NOT EXISTS %s (", name);

	if (sql == NULL) {
		ret = JARY_ERR_OOM;
		goto FINISH;
	}

	for (size_t i = 0; i < event->capacity; ++i) {
		const char *type   = NULL;
		const char *column = event->keys[i];

		if (column == NULL)
			continue;

		switch (event->types[i]) {
		case JY_K_STR:
			type = "TEXT";
			break;
		case JY_K_LONG:
			type = "INTEGER";
			break;
		default:
			continue;
		}

		if (strcmp(column, "__arrival__") == 0)
			type = "INTEGER DEFAULT (unixepoch())";

		sqlsz = sc_strfmt(&m, &sql, "%s%s %s,", sql, column, type);

		if (sql == NULL) {
			ret = JARY_ERR_OOM;
			goto FINISH;
		}
	}

	assert(sqlsz > 0);

	sql[sqlsz - 1] = ')';

	sc_strfmt(&m, &sql, "%s;", sql);

	if (sql == NULL) {
		ret = JARY_ERR_OOM;
		goto FINISH;
	}

	switch (sqlite3_exec(db, sql, NULL, NULL, NULL)) {
	case SQLITE_OK:
		break;
	default:
		ret = JARY_ERR_SQLITE3;
	}

FINISH:
	sc_free(&m);
	return ret;
}

static inline int insert_event(struct sqlite3 *db,
			       const char     *name,
			       uint32_t	       length,
			       const char    **keys,
			       const char    **values)
{
	int	      ret = JARY_OK;
	struct sc_mem m	  = { .buf = NULL };

	char *sql;
	int   sqlsz = 0;

	sc_strfmt(&m, &sql, "INSERT INTO %s (", name);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (uint32_t i = 0; i < length; ++i) {
		const char *column = keys[i];

		if (column == NULL)
			continue;

		sqlsz = sc_strfmt(&m, &sql, "%s%s,", sql, column);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	assert(sqlsz > 0);

	sql[sqlsz - 1] = ')';

	sc_strfmt(&m, &sql, "%s VALUES (", sql);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (uint32_t i = 0; i < length; ++i) {
		const char *value = values[i];

		if (value == NULL)
			continue;

		sqlsz = sc_strfmt(&m, &sql, "%s%s,", sql, value);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	assert(sqlsz > 0);

	sql[sqlsz - 1] = ')';

	sc_strfmt(&m, &sql, "%s;", sql);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	switch (sqlite3_exec(db, sql, NULL, NULL, NULL)) {
	case SQLITE_OK:
		break;
	default:
		ret = JARY_ERR_SQLITE3;
	}

	goto FINISH;

OUT_OF_MEMORY:
	ret = JARY_ERR_OOM;

FINISH:
	sc_free(&m);
	return ret;
}

static inline int rule_clbks(size_t		    rule,
			     const struct jyOutput *output,
			     size_t		    length,
			     const uint16_t	   *ords,
			     void *const	   *datas,
			     int (*const *clbks)(void *data,
						 const struct jyOutput *))
{
	for (size_t i = 0; i < length; ++i) {
		if (ords[i] != rule)
			continue;

		void *data = datas[i];
		switch (clbks[i](data, output)) {
		case JARY_INT_CRASH:
			return JARY_INT_CRASH;
		case JARY_INT_FINAL:
			goto FINISH;
		};
	}

FINISH:
	return JARY_OK;
}

int jary_open(struct jary **jary, struct sqlite3 *db)
{
	int	     ret;
	struct jary *J = calloc(sizeof(struct jary), 1);

	if (J == NULL)
		goto OUT_OF_MEMORY;

	J->mdir = "";

	if (db == NULL) {
		int flag = SQLITE_OPEN_MEMORY | SQLITE_OPEN_PRIVATECACHE
			 | SQLITE_OPEN_READWRITE;
		if (sqlite3_open_v2("dumb.db", &J->db, flag, NULL))
			goto OPEN_ERROR;
	} else {
		J->db = db;
	}

	if (sc_reap(&J->sc, &J->sb, (free_t) sb_free))
		goto OUT_OF_MEMORY;

	if (sc_reap(&J->sc, &J->ev_colsz, (free_t) ifree))
		goto OUT_OF_MEMORY;

	if (sc_reap(&J->sc, &J->ev_tables, (free_t) ifree))
		goto OUT_OF_MEMORY;

	if (sc_reap(&J->sc, &J->ev_cols, (free_t) ifree))
		goto OUT_OF_MEMORY;

	if (sc_reap(&J->sc, &J->ev_vals, (free_t) ifree))
		goto OUT_OF_MEMORY;

	if (sc_reap(&J->sc, &J->r_clbk_datas, (free_t) ifree))
		goto OUT_OF_MEMORY;

	if (sc_reap(&J->sc, &J->r_clbk_ords, (free_t) ifree))
		goto OUT_OF_MEMORY;

	if (sc_reap(&J->sc, &J->r_clbks, (free_t) ifree))
		goto OUT_OF_MEMORY;

	*jary = J;
	return JARY_OK;

OPEN_ERROR:
	ret = JARY_ERROR;
	// TODO: Handle close?
	sqlite3_close_v2(J->db);
	goto PANIC;

OUT_OF_MEMORY:
	ret = JARY_ERR_OOM;

PANIC:
	free(J);
	*jary = NULL;

	return ret;
}

int jary_modulepath(struct jary *jary, const char *path)
{
	struct sc_mem *sc = &jary->sc;

	// TODO: if a user keep changing module path, those allocated pointers
	// will live until jary is closed. This is kinda wasteful but meh.
	sc_strfmt(sc, &jary->mdir, "%s", path);

	if (jary->mdir == NULL) {
		jary->errmsg = "out of memory";
		return JARY_ERR_OOM;
	}

	return JARY_OK;
}

int jary_event(struct jary *J, const char *name, unsigned int *event)
{
	int ret = JARY_OK;

	if (J->code == NULL)
		return JARY_ERR_NOTEXIST;

	assert(J->code->jay);
	assert(J->code->jay->names);

	struct jy_defs *names = J->code->jay->names;
	struct sc_mem  *sc    = &J->sc;

	if (!def_find(names, name, NULL)) {
		J->errmsg = "event not expected";
		return JARY_ERR_NOTEXIST;
	}

	char *table = NULL;

	sc_strfmt(sc, &table, "%s", name);

	if (table == NULL)
		return JARY_ERR_OOM;

	jry_mem_push(J->ev_tables, J->ev_sz, table);

	if (J->ev_tables == NULL)
		goto OUT_OF_MEMORY;

	jry_mem_push(J->ev_colsz, J->ev_sz, 0);

	if (J->ev_colsz == NULL)
		goto OUT_OF_MEMORY;

	jry_mem_push(J->ev_cols, J->ev_sz, NULL);

	if (J->ev_cols == NULL)
		goto OUT_OF_MEMORY;

	jry_mem_push(J->ev_vals, J->ev_sz, NULL);

	if (J->ev_vals == NULL)
		goto OUT_OF_MEMORY;

	goto FINISH;

OUT_OF_MEMORY:
	ret = JARY_ERR_OOM;

FINISH:
	*event	  = J->ev_sz;
	J->ev_sz += 1;

	return ret;
}

int jary_field_str(struct jary *jary,
		   unsigned int event,
		   const char  *field,
		   const char  *value)
{
	assert(event <= jary->ev_sz);

	const char     *table = jary->ev_tables[event];
	struct sc_mem  *sc    = &jary->sc;
	struct jy_defs *names = jary->code->jay->names;
	union jy_value	view;
	enum jy_ktype	type;

	assert(def_get(names, table, &view, NULL) == JARY_OK);

	if (def_get(view.def, field, NULL, &type)) {
		jary->errmsg = "field not expected";
		return JARY_ERR_NOTEXIST;
	}

	if (type != JY_K_STR) {
		jary->errmsg = "not a field string";
		return JARY_ERR_MISMATCH;
	}

	char *col;
	char *val;

	uint8_t	     colsz = jary->ev_colsz[event];
	const char **cols  = jary->ev_cols[event];
	const char **vals  = jary->ev_vals[event];

	sc_strfmt(sc, &col, "%s", field);

	if (col == NULL)
		goto OUT_OF_MEMORY;

	val = sqlite3_mprintf("%Q", value);

	if (val == NULL)
		goto OUT_OF_MEMORY;

	if (sc_reap(sc, val, sqlite3_free))
		goto OUT_OF_MEMORY;

	for (size_t i = 0; i < colsz; ++i) {
		const char *f = cols[i];

		if (*f == *field && strcmp(field, f)) {
			cols[i] = field;
			vals[i] = value;
			goto FINISH;
		}
	}

	jry_mem_push(vals, colsz, val);

	if (vals == NULL)
		goto OUT_OF_MEMORY;

	jry_mem_push(cols, colsz, col);

	if (cols == NULL)
		goto OUT_OF_MEMORY;

	goto FINISH;

OUT_OF_MEMORY:
	jary->errmsg = "out of memory";

FINISH:
	colsz		      += 1;
	jary->ev_colsz[event]  = colsz;
	jary->ev_cols[event]   = cols;
	jary->ev_vals[event]   = vals;
	return JARY_OK;
}

int jary_compile_file(struct jary *jary, const char *path)
{
	struct sc_mem sc = { .buf = NULL };
	char	     *src;
	uint32_t      srcsz;

	int   ret  = JARY_OK;
	FILE *file = fopen(path, "rb");

	if (file == NULL)
		goto OPEN_FAIL;

	fseek(file, 0L, SEEK_END);
	srcsz = ftell(file);
	rewind(file);

	src		    = sc_alloc(&sc, srcsz + 1);
	uint32_t bytes_read = fread(src, sizeof(char), srcsz, file);

	if (bytes_read < srcsz)
		goto READ_FAIL;

	src[bytes_read] = '\0';

	ret = jary_compile(jary, srcsz, src);
	goto FINISH;

OPEN_FAIL:
	jary->errmsg = "unable to open jary file";
	return JARY_ERROR;

READ_FAIL:
	jary->errmsg = "unable to read jary file";
	ret	     = JARY_ERROR;
FINISH:
	sc_free(&sc);
	fclose(file);
	return ret;
}

int jary_compile(struct jary *jary, size_t length, const char *source)
{
	if (jary->code != NULL) {
		jary->errmsg = "jary context already compiled";
		return JARY_ERR_COMPILE;
	}

	struct sb_mem *sb = &jary->sb;

	struct exec *code = sc_alloc(&jary->sc, sizeof(*code));

	size_t memsz = sizeof(*code->asts) + sizeof(*code->tkns)
		     + sizeof(*code->jay) + sizeof(*code->errs);

	if (sb_reserve(sb, SB_NOGROW, memsz) == NULL)
		return JARY_ERR_OOM;

	struct jy_tkns	*tkns;
	struct jy_asts	*asts;
	struct jy_jay	*jay;
	struct tkn_errs *errs;

	int flag = SB_NOREGEN;

	tkns = sb_append(sb, flag, sizeof(*code->tkns));
	assert(tkns != NULL);

	asts = sb_append(sb, flag, sizeof(*code->asts));
	assert(asts != NULL);

	jay = sb_append(sb, flag, sizeof(*code->jay));
	assert(jay != NULL);

	errs = sb_append(sb, flag, sizeof(*code->errs));
	assert(errs != NULL);

	code->tkns = tkns;
	code->asts = asts;
	code->jay  = jay;
	code->errs = errs;

	jry_parse(&jary->sc, asts, tkns, errs, source, length);

	if (code->errs->size)
		return JARY_ERR_COMPILE;

	jry_compile(&jary->sc, jay, errs, jary->mdir, asts, tkns);

	if (code->errs->size)
		return JARY_ERR_COMPILE;

	struct jy_defs *names	= code->jay->names;
	size_t		eventsz = 0;
	const char     *table[names->size];
	struct jy_defs *events[names->size];

	for (size_t i = 0; i < names->capacity; ++i) {
		switch (names->types[i]) {
		case JY_K_EVENT:
			table[eventsz]	 = names->keys[i];
			events[eventsz]	 = names->vals[i].def;
			eventsz		+= 1;
			break;
		default:
			continue;
		}
	}

	for (size_t i = 0; i < eventsz; ++i)
		if (create_event_table(jary->db, table[i], events[i]))
			return JARY_ERROR;

	jary->code = code;

	return JARY_OK;
}

void jary_output_len(const struct jyOutput *output, unsigned int *length)
{
	*length = output->size;
}

int jary_output_str(const struct jyOutput *output,
		    unsigned int	   index,
		    const char		 **str)
{
	if (index >= output->size)
		return JARY_ERR_NOTEXIST;

	*str = output->values[index].str->cstr;

	return JARY_OK;
}

int jary_output_long(const struct jyOutput *output,
		     unsigned int	    index,
		     long		   *num)
{
	if (index >= output->size)
		return JARY_ERR_NOTEXIST;

	*num = output->values[index].i64;

	return JARY_OK;
}

int jary_output_ulong(const struct jyOutput *output,
		      unsigned int	     index,
		      unsigned long	    *num)
{
	if (index >= output->size)
		return JARY_ERR_NOTEXIST;

	*num = output->values[index].u64;

	return JARY_OK;
}

int jary_output_bool(const struct jyOutput *output,
		     unsigned int	    index,
		     bool		   *boolean)
{
	if (index >= output->size)
		return JARY_ERR_NOTEXIST;

	*boolean = output->values[index].u64;

	return JARY_OK;
}

int jary_rule_clbk(struct jary *jary,
		   const char  *name,
		   int (*callback)(void *, const struct jyOutput *),
		   void *data)
{
	uint32_t	ordinal = 0;
	struct jy_defs *names	= jary->code->jay->names;
	union jy_value	view;
	enum jy_ktype	type;

	if (def_get(names, name, &view, &type))
		return JARY_ERR_NOTEXIST;

	if (type != JY_K_RULE)
		return JARY_ERR_NOTEXIST;

	ordinal = view.ofs;

	jry_mem_push(jary->r_clbks, jary->r_clbk_sz, callback);

	if (jary->r_clbks == NULL)
		return JARY_ERR_OOM;

	jry_mem_push(jary->r_clbk_ords, jary->r_clbk_sz, ordinal);

	if (jary->r_clbk_ords == NULL)
		return JARY_ERR_OOM;

	jry_mem_push(jary->r_clbk_datas, jary->r_clbk_sz, data);

	if (jary->r_clbk_datas == NULL)
		return JARY_ERR_OOM;

	jary->r_clbk_sz += 1;
	return JARY_OK;
}

int jary_execute(struct jary *jary)
{
	assert(jary->code != NULL);

	int ret = JARY_OK;

	const struct jy_jay *jay    = jary->code->jay;
	struct sc_mem	     sc	    = { .buf = NULL };
	struct sb_mem	     outmem = { .buf = NULL };

	if (sc_reap(&sc, &outmem, (free_t) sb_free))
		goto OUT_OF_MEMORY;

	for (unsigned int i = 0; i < jary->ev_sz; ++i) {
		const char  *table = jary->ev_tables[i];
		const char **cols  = jary->ev_cols[i];
		const char **vals  = jary->ev_vals[i];
		uint8_t	     colsz = jary->ev_colsz[i];

		if (insert_event(jary->db, table, colsz, cols, vals))
			goto INSERT_FAIL;
	}

	const uint16_t *ords   = jary->r_clbk_ords;
	void *const    *datas  = jary->r_clbk_datas;
	size_t		clbksz = jary->r_clbk_sz;
	int (*const *clbks)(void *, const struct jyOutput *) = jary->r_clbks;

	for (size_t i = 0; i < jay->rulesz; ++i) {
		struct jy_state state = { .buf = &sc, .outm = &outmem };
		size_t		ofs   = jay->rulecofs[i];
		uint8_t	       *code  = jay->codes + ofs;

		switch (jry_exec(jary->db, jay, code, &state)) {
		case 1:
			goto OUT_OF_MEMORY;
		case 2:
			goto QUERY_FAILED;
		}

		struct jyOutput output = {
			.size	= state.outsz,
			.values = state.out,
		};

		switch (rule_clbks(i, &output, clbksz, ords, datas, clbks)) {
		case JARY_INT_CRASH:
			goto FINISH;
		};
	}

	goto FINISH;

OUT_OF_MEMORY:
	jary->errmsg = "out of memory";
	ret	     = JARY_ERR_OOM;
	goto FINISH;

INSERT_FAIL:
	jary->errmsg = "unable to process event queue";
	ret	     = JARY_ERR_EXEC;
	goto FINISH;

QUERY_FAILED:
	jary->errmsg = "unable to perform query. report this bug";
	ret	     = JARY_ERR_EXEC;

FINISH:
	for (uint32_t i = 0; i < jary->ev_sz; ++i) {
		free(jary->ev_cols[i]);
		free(jary->ev_vals[i]);
	}

	jary->ev_sz = 0;

	sc_free(&sc);
	return ret;
}

int jary_close(struct jary *restrict jary)
{
	switch (sqlite3_close_v2(jary->db)) {
	case SQLITE_OK:
		break;
	default:
		jary->errmsg = sqlite3_errmsg(jary->db);
		return JARY_ERROR;
	};

	for (uint32_t i = 0; i < jary->ev_sz; ++i) {
		free(jary->ev_cols[i]);
		free(jary->ev_vals[i]);
	}

	sc_free(&jary->sc);

	free(jary);

	return JARY_OK;
}

const char *jary_errmsg(struct jary *jary)
{
	return jary->errmsg;
}
