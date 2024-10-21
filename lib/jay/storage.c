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

#include "storage.h"

#include "jary/memory.h"

#include <assert.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <string.h>

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

int q_match(struct sqlite3 *db,
	    char	  **errmsg,
	    int (*callback)(void *, int, char **, char **),
	    void	 *data,
	    struct Qmatch Q)
{
	struct sc_mem buf = { .buf = NULL };

	int		qlen = Q.qlen;
	struct QMbase **qs   = Q.qlist;

	char		     *sql;
	const struct QMjoin  *joins[qlen];
	const struct QMexact *exacts[qlen];
	const struct QMbase  *uniq[qlen * 2];

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

	for (int i = 0; i < joinsz; ++i) {
		const struct QMjoin *J = joins[i];
		struct QMbase	    *Q = sc_alloc(&buf, sizeof *Q);
		Q->table	       = J->tbl_right;
		Q->column	       = J->col_right;

		if (!exists(uniqsz, uniq, Q)) {
			uniq[i + uniqsz]  = Q;
			uniqsz		 += 1;
		}
	}

	assert(joinsz > 0 || exactsz > 0);

	sc_strfmt(&buf, &sql, "SELECT");

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < uniqsz; ++i) {
		const char *t = uniq[i]->table;
		const char *c = uniq[i]->column;

		char *fmt;

		if (i + 1 < uniqsz)
			fmt = "%s %s.%s AS \"%s.%s\",";
		else
			fmt = "%s %s.%s AS \"%s.%s\" FROM";

		sc_strfmt(&buf, &sql, fmt, sql, t, c, t, c);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	for (int i = 0; i < uniqsz; ++i) {
		const char *t = uniq[i]->table;

		if (i + 1 < uniqsz)
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

	if (sqlite3_exec(db, sql, callback, data, errmsg) != SQLITE_OK)
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
