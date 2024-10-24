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

// THESE ARE SUCH A BAD IMPLEMENTATION, BUT IM STRAPPED FOR TIME!!!

#ifndef JAYVM_Q_H
#define JAYVM_Q_H

#include "jary/defs.h"
#include "jary/memory.h"

#include <assert.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <string.h>

typedef int (*q_callback_t)(void *, int, char **, char **);

struct sqlite3;
struct jy_defs;
struct jy_time_ofs;

enum QMtag {
	QM_NONE = 0,
	QM_EXACT,
	QM_JOIN,
	QM_WITHIN,
	QM_BETWEEN,
};

struct QMbase {
	enum QMtag  type;
	const char *table;
	const char *column;
};

struct QMequal {
	enum QMtag  type;
	const char *table;
	const char *column;

	struct {
		enum {
			QME_LONG,
			QME_CSTR,
		} type;

		union {
			int64_t	    i64;
			const char *cstr;
		} as;
	} value;
};

struct QMbetween {
	enum QMtag  type;
	const char *table;
	const char *column;
	long	    min;
	long	    max;
};

struct QMwithin {
	enum QMtag	   type;
	const char	  *table;
	const char	  *column;
	struct jy_time_ofs timeofs;
};

struct QMjoin {
	enum QMtag  type;
	const char *tbl_left;
	const char *col_left;
	const char *tbl_right;
	const char *col_right;
};

struct Qmatch {
	int		qlen;
	struct QMbase **qlist;
	struct jy_defs *names;
};

static inline bool exists(int			 length,
			  const struct jy_defs **list,
			  const struct jy_defs	*def)
{
	for (int i = 0; i < length; ++i)
		if (list[i] == def)
			return true;

	return false;
}

static inline int q_match(struct sqlite3 *db,
			  char		**errmsg,
			  q_callback_t	  callback,
			  void		 *data,
			  struct Qmatch	  Q)
{
	int	      sz  = 0;
	struct sc_mem buf = { .buf = NULL };

	int		qlen  = Q.qlen;
	struct QMbase **qs    = Q.qlist;
	struct jy_defs *names = Q.names;

	char		       *sql;
	const struct QMjoin    *joins[qlen];
	const struct QMequal   *exacts[qlen];
	const struct QMbetween *between[qlen];
	const struct QMwithin  *within[qlen];
	const struct jy_defs   *events[qlen * 2];
	const char	       *eventnames[qlen * 2];

	int eventsz   = 0;
	int joinsz    = 0;
	int exactsz   = 0;
	int withinsz  = 0;
	int betweensz = 0;

	memset(joins, 0, sizeof(joins));
	memset(exacts, 0, sizeof(exacts));
	memset(events, 0, sizeof(events));

	for (int i = 0; i < qlen; ++i) {
		const struct QMbase *Q = qs[i];

		union jy_value event;
		enum jy_ktype  type;

		assert(def_get(names, Q->table, &event, &type) == 0);
		assert(type == JY_K_EVENT);

		if (!exists(eventsz, events, event.def)) {
			events[eventsz]	     = event.def;
			eventnames[eventsz]  = Q->table;
			eventsz		    += 1;
		}

		switch (Q->type) {
		case QM_NONE:
			assert(Q->type != QM_NONE);
			break;
		case QM_EXACT:
			exacts[exactsz]	 = (struct QMequal *) Q;
			exactsz		+= 1;
			break;
		case QM_JOIN:
			joins[joinsz]  = (struct QMjoin *) Q;
			joinsz	      += 1;
			break;
		case QM_WITHIN:
			within[withinsz]  = (struct QMwithin *) Q;
			withinsz	 += 1;
			break;
		case QM_BETWEEN:
			between[betweensz]  = (struct QMbetween *) Q;
			betweensz	   += 1;
			break;
		}
	}

	for (int i = 0; i < joinsz; ++i) {
		const struct QMjoin *J = joins[i];

		union jy_value event;
		enum jy_ktype  type;

		assert(def_get(names, J->tbl_right, &event, &type) == 0);
		assert(type == JY_K_EVENT);

		if (!exists(eventsz, events, event.def)) {
			events[eventsz]	     = event.def;
			eventnames[eventsz]  = J->tbl_right;
			eventsz		    += 1;
		}
	}

	assert(joinsz > 0 || exactsz > 0 || withinsz > 0 || betweensz > 0);

	sc_strfmt(&buf, &sql, "SELECT");

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < eventsz; ++i) {
		const struct jy_defs *event = events[i];
		char		     *keys[eventsz];

		const char *t	= eventnames[i];
		int	    len = def_keys(event, event->size, keys);

		const char fmt[] = "%s %s.%s AS \"%s.%s\",";

		for (int j = 0; j < len; ++j) {
			const char *c = keys[j];
			sz = sc_strfmt(&buf, &sql, fmt, sql, t, c, t, c);
		}

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	sql[sz - 1] = ' ';

	sc_strfmt(&buf, &sql, "%sFROM", sql);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	for (int i = 0; i < eventsz; ++i) {
		const char *tbl = eventnames[i];

		if (i + 1 < eventsz)
			sz = sc_strfmt(&buf, &sql, "%s %s,", sql, tbl);
		else
			sz = sc_strfmt(&buf, &sql, "%s %s WHERE", sql, tbl);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	for (int i = 0; i < joinsz; ++i) {
		const struct QMjoin *Q	= joins[i];
		const char	    *lt = Q->tbl_left;
		const char	    *lc = Q->col_left;
		const char	    *rt = Q->tbl_right;
		const char	    *rc = Q->col_right;

		const char *fmt = "%s %s.%s = %s.%s AND";

		sz = sc_strfmt(&buf, &sql, fmt, sql, lt, lc, rt, rc);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	for (int i = 0; i < exactsz; ++i) {
		const struct QMequal *Q	 = exacts[i];
		const char	     *lt = Q->table;
		const char	     *lc = Q->column;
		const char	     *fmt;

		switch (Q->value.type) {
		case QME_CSTR: {
			const char *r = Q->value.as.cstr;
			// TODO: potential SQL injection? maybe later...
			fmt	      = "%s %s.%s = '%s' AND";
			sz = sc_strfmt(&buf, &sql, fmt, sql, lt, lc, r);
			break;
		}
		case QME_LONG: {
			long r = Q->value.as.i64;
			fmt    = "%s %s.%s = '%ld' AND";
			sz     = sc_strfmt(&buf, &sql, fmt, sql, lt, lc, r);
			break;
		}
		}

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	for (int i = 0; i < withinsz; ++i) {
		const struct QMwithin *Q = within[i];
		long ofs		 = Q->timeofs.offset * Q->timeofs.time;

		const char fmt[] = "%s unixepoch() - %s.%s <= %ld AND";

		sz = sc_strfmt(&buf, &sql, fmt, sql, Q->table, Q->column, ofs);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	for (int i = 0; i < betweensz; ++i) {
		const struct QMbetween *Q   = between[i];
		const char	       *t   = Q->table;
		const char	       *c   = Q->column;
		long			min = Q->min;
		long			max = Q->max;

		const char fmt[] = "%s %s.%s > %ld AND %s.%s < %ld AND";

		sz = sc_strfmt(&buf, &sql, fmt, sql, t, c, min, t, c, max);

		if (sql == NULL)
			goto OUT_OF_MEMORY;
	}

	sql[sz - 4] = ';';
	sql[sz - 3] = '\0';

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

#endif // JAYVM_Q_H
