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
#include "parser.h"
#include "token.h"

#include "jary/defs.h"
#include "jary/memory.h"

#include <assert.h>
#include <sqlite3.h>
#include <string.h>

struct jarycode {
	struct jy_tkns	*tkns;
	struct jy_asts	*asts;
	struct jy_jay	*jay;
	struct tkn_errs *errs;
	char		*mdir;
	struct sb_mem	*sb;
};

struct jary {
	struct sc_mem	 sc;
	// TODO: use this for storing error msg
	/*const char		*errmsg;*/
	struct jarycode *code;
	struct sqlite3	*db;
};

static inline int event_table(struct sqlite3	   *db,
			      const char	   *name,
			      const struct jy_defs *event)
{
	int	      ret = JARY_OK;
	struct sc_mem m	  = { .buf = NULL };

	char *sql;
	int   sqlsz = 0;

	sc_strfmt(&m, &sql, "CREATE TABLE %s (", name);

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

int jary_open(struct jary **jary)
{
	int	      ret;
	struct jary  *J;
	struct sc_mem m = { .buf = NULL };

	J = sc_alloc(&m, sizeof *J);

	if (J == NULL)
		goto OUT_OF_MEMORY;

	J->sc = m;

	int flag = SQLITE_OPEN_MEMORY | SQLITE_OPEN_PRIVATECACHE
		 | SQLITE_OPEN_READWRITE;
	if (sqlite3_open_v2("dumb.db", &J->db, flag, NULL))
		goto OPEN_ERROR;

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
	sc_free(&m);
	*jary = NULL;

	return ret;
}

int jary_insert_event(struct jary *restrict jary,
		      const char  *name,
		      int	   length,
		      const char **keys,
		      const char **values)
{
	assert(jary != NULL);
	assert(jary->code != NULL);

	int	      ret = JARY_OK;
	struct sc_mem m	  = { .buf = NULL };

	char *sql;
	int   sqlsz = 0;

	sc_strfmt(&m, &sql, "INSERT INTO %s (", name);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < length; ++i) {
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

	for (int i = 0; i < length; ++i) {
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

	switch (sqlite3_exec(jary->db, sql, NULL, NULL, NULL)) {
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

int jary_compile(struct jary *restrict jary,
		 size_t	     length,
		 const char *source,
		 const char *modulepath)
{
	// TODO: write error message
	if (jary->code != NULL)
		return JARY_ERR_COMPILE;

	struct sb_mem *sb = sc_alloc(&jary->sc, sizeof(*sb));

	if (sb == NULL)
		return JARY_ERR_OOM;

	if (sc_reap(&jary->sc, sb, (free_t) sb_free))
		return JARY_ERR_OOM;

	struct jarycode *code = sc_alloc(&jary->sc, sizeof(*code));

	size_t mdirsz = strlen(modulepath) + 1;
	size_t memsz  = mdirsz;
	memsz += sizeof(*code->asts) + sizeof(*code->tkns) + sizeof(*code->jay)
	       + sizeof(*code->errs);

	if (sb_reserve(sb, SB_NOGROW, memsz) == NULL)
		return JARY_ERR_OOM;

	int flag   = SB_NOREGEN;
	code->tkns = sb_append(sb, flag, sizeof(*code->tkns));
	assert(code->tkns != NULL);

	code->asts = sb_append(sb, flag, sizeof(*code->asts));
	assert(code->asts != NULL);

	code->jay = sb_append(sb, flag, sizeof(*code->jay));
	assert(code->jay != NULL);

	code->errs = sb_append(sb, flag, sizeof(*code->errs));
	assert(code->jay != NULL);

	code->mdir = sb_append(sb, flag, mdirsz);
	assert(code->mdir != NULL);

	code->sb = sb;

	jry_parse(&jary->sc, code->asts, code->tkns, code->errs, source,
		  length);

	if (code->errs->size)
		return JARY_ERR_COMPILE;

	jry_compile(&jary->sc, code->jay, code->errs, modulepath, code->asts,
		    code->tkns);

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
		if (event_table(jary->db, table[i], events[i]))
			return JARY_ERROR;

	jary->code = code;

	return JARY_OK;
}

int jary_close(struct jary *restrict jary)
{
	// TODO: handle each error accordingly
	switch (sqlite3_close_v2(jary->db)) {
	case SQLITE_OK:
		break;
	default:
		return JARY_ERROR;
	};

	sc_free(&jary->sc);

	return JARY_OK;
}
