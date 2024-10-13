#include "storage.h"

#include "jary/memory.h"

#include <assert.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TYPE_MASK 0xf

static inline bool exists(int			length,
			  const struct QMbase **qlist,
			  const struct QMbase  *Q)
{
	for (int i = 0; i < length; ++i) {
		if (strcmp(qlist[i]->table, Q->table))
			continue;

		if (strcmp(qlist[i]->column, Q->column) == 0)
			return true;
	}

	return false;
}

int q_match(struct sqlite3 *db, struct Qmatch Q)
{
	struct sc_mem buf    = { .buf = NULL };

	int		qlen = Q.qlen;
	struct QMbase **qs   = Q.qlist;

	char		     *sql;
	const struct QMjoin  *joins[qlen];
	const struct QMexact *exacts[qlen];
	const struct QMbase  *uniq[qlen];

	int uniqsz  = 0;
	int joinsz  = 0;
	int exactsz = 0;

	memset(joins, 0, sizeof(joins));
	memset(exacts, 0, sizeof(exacts));
	memset(uniq, 0, sizeof(uniq));

	for (int i = 0; i < qlen; ++i) {
		const struct QMbase *Q = qs[i];

		if (!exists(uniqsz, uniq, Q)) {
			uniq[i]	 = Q;
			uniqsz	+= 1;
		}

		switch (Q->type) {
		case QM_EXACT:
			exacts[i]  = (struct QMexact *) Q;
			exactsz	  += 1;
			break;
		case QM_JOIN:
			joins[i]  = (struct QMjoin *) Q;
			joinsz	 += 1;
			break;
		}
	}

	assert(joinsz > 0 || exactsz > 0);

	sc_strfmt(&buf, &sql, "SELECT");

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < uniqsz; ++i) {
		const char *t = uniq[i]->table;
		const char *c = uniq[i]->column;

		if (i + 1 > uniqsz)
			sc_strfmt(&buf, &sql, "%s %s.%s,", sql, t, c);
		else
			sc_strfmt(&buf, &sql, "%s %s.%s FROM", sql, t, c);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	for (int i = 0; i < uniqsz; ++i) {
		const char *t = uniq[i]->table;

		if (i + 1 > uniqsz)
			sc_strfmt(&buf, &sql, "%s %s,", sql, t);
		else
			sc_strfmt(&buf, &sql, "%s %s WHERE", sql, t);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	for (int i = 0; i < joinsz; ++i) {
		const struct QMjoin *Q	 = joins[i];
		const char	    *lt	 = Q->tbl_left;
		const char	    *lc	 = Q->col_left;
		const char	    *rt	 = Q->tbl_right;
		const char	    *rc	 = Q->col_right;
		char		    *fmt = "%s %s.%s = %s.%s,";

		sc_strfmt(&buf, &sql, fmt, sql, lt, lc, rt, rc);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	if (exactsz == 0)
		goto CLOSE;

	for (int i = 0; i < exactsz; ++i) {
		const struct QMexact *Q	 = exacts[i];
		const char	     *lt = Q->table;
		const char	     *lc = Q->column;
		const char	     *r	 = Q->value;

		// TODO: potential SQL injection? maybe later...
		sc_strfmt(&buf, &sql, "%s %s.%s = \"%s\",", sql, lt, lc, r);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

CLOSE: {
	uint32_t sz = strlen(sql);
	sql[sz - 1] = ';';
}

	if (sqlite3_exec(db, sql, Q.callback, NULL, NULL) != SQLITE_OK)
		goto TX_FAILED;

	sc_free(&buf);
	return 0;
OUT_OF_MEMORY:
	sc_free(&buf);
	return -1;
TX_FAILED:
	sc_free(&buf);
	return -2;
}

int q_create(struct sqlite3 *db, struct Qcreate Q)
{
	const char  *name    = Q.table;
	int	     length  = Q.colsz;
	const char **columns = Q.columns;
	int	    *cflags  = Q.flags;

	struct sc_mem buf    = { .buf = NULL };

	char *sql;
	sc_strfmt(&buf, &sql, "CREATE TABLE IF NOT EXISTS %s (", name);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < length; ++i) {
		const char *c	 = columns[i];
		int	    f	 = cflags[i];
		int	    type = f & TYPE_MASK;

		char *tstr;

		switch (type) {
		case Q_COL_INT:
			tstr = "INTEGER";
			break;
		case Q_COL_STR:
		default:
			tstr = "TEXT";
		}

		if (i + 1 > length)
			sc_strfmt(&buf, &sql, "%s %s %s,", sql, c, tstr);
		else
			sc_strfmt(&buf, &sql, "%s %s %s);", sql, c, tstr);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK)
		goto TX_FAILED;

	sc_free(&buf);
	return 0;
OUT_OF_MEMORY:
	sc_free(&buf);
	return -1;
TX_FAILED:
	sc_free(&buf);
	return -2;
}

int q_insert(struct sqlite3 *db, struct Qinsert Q)
{
	const char   *name    = Q.table;
	int	      length  = Q.colsz;
	const char  **columns = Q.columns;
	const char  **values  = Q.values;
	struct sc_mem buf     = { .buf = NULL };
	char	     *sql     = NULL;

	sc_strfmt(&buf, &sql, "INSERT INTO %s (", name);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < length; ++i) {
		if (i + 1 > length)
			sc_strfmt(&buf, &sql, "%s %s,", sql, columns[i]);
		else
			sc_strfmt(&buf, &sql, "%s %s)", sql, columns[i]);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	sc_strfmt(&buf, &sql, "%s VALUES(");

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < length; ++i) {
		if (i + 1 > length)
			sc_strfmt(&buf, &sql, "%s %s,", sql, values[i]);
		else
			sc_strfmt(&buf, &sql, "%s %s);", sql, values[i]);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK)
		goto TX_FAILED;

	sc_free(&buf);
	return 0;
OUT_OF_MEMORY:
	sc_free(&buf);
	return -1;
TX_FAILED:
	sc_free(&buf);
	return -2;
}
