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
#include <stdio.h>
#include <string.h>

typedef int(q_clbk)(void *, int, char **, char **);

struct sqlite3;
struct jy_defs;
struct jy_time_ofs;

enum QMtag {
	QM_NONE = 0,
	QM_BINARY,
	QM_JOIN,
	QM_WITHIN,
	QM_BETWEEN,
};

struct QMbase {
	enum QMtag  type;
	const char *table;
	const char *column;
};

struct QMbinary {
	enum QMtag  type;
	const char *table;
	const char *column;

	struct {
		enum {
			QME_LONG,
			QME_CSTR,
			QME_REGEXP,
		} type;

		union {
			int64_t	    i64;
			const char *cstr;
			const char *regex;
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

static inline int prslcq(int bufsz,
			 char *restrict buf,
			 int			  eventsz,
			 int			  joinsz,
			 int			  binsz,
			 int			  withinsz,
			 int			  betweensz,
			 const struct jy_defs	**events,
			 const char		**evnames,
			 const struct QMjoin	**joins,
			 const struct QMbinary	**binary,
			 const struct QMbetween **between,
			 const struct QMwithin	**within)
{
#define SIZE() bufsz ? bufsz - sz : 0
#define PTR()  sz ? buf + sz : buf
	int	      sz   = 0;
	struct sc_mem bump = { .buf = NULL };

	sz += snprintf(PTR(), SIZE(), "SELECT");

	for (int i = 0; i < eventsz; ++i) {
		const struct jy_defs *event = events[i];
		char **keys = sc_alloc(&bump, sizeof(void *) * event->size);

		if (keys == NULL)
			goto OUT_OF_MEMORY;

		const char *t	= evnames[i];
		int	    len = def_keys(event, event->size, keys);

		const char fmt[] = " %s.%s AS '%s.%s',";

		for (int j = 0; j < len; ++j) {
			const char *c = keys[j];
			sz += snprintf(PTR(), SIZE(), fmt, t, c, t, c);
		}
	}

	if (buf)
		buf[sz - 1] = ' ';

	sz += snprintf(PTR(), SIZE(), "FROM");

	for (int i = 0; i < eventsz; ++i) {
		const char *tbl = evnames[i];

		if (i + 1 < eventsz)
			sz += snprintf(PTR(), SIZE(), " %s,", tbl);
		else
			sz += snprintf(PTR(), SIZE(), " %s WHERE", tbl);
	}

	for (int i = 0; i < joinsz; ++i) {
		const struct QMjoin *Q	= joins[i];
		const char	    *lt = Q->tbl_left;
		const char	    *lc = Q->col_left;
		const char	    *rt = Q->tbl_right;
		const char	    *rc = Q->col_right;

		const char *fmt = " %s.%s = %s.%s AND";

		sz += snprintf(PTR(), SIZE(), fmt, lt, lc, rt, rc);
	}

	for (int i = 0; i < binsz; ++i) {
		const struct QMbinary *Q  = binary[i];
		const char	      *lt = Q->table;
		const char	      *lc = Q->column;
		const char	      *fmt;

		switch (Q->value.type) {
		case QME_REGEXP: {
			const char *r = Q->value.as.regex;

			// TODO: potential SQL injection? maybe later...
			fmt  = " %s.%s REGEXP '%s' AND";
			sz  += snprintf(PTR(), SIZE(), fmt, lt, lc, r);
			break;
		}

		case QME_CSTR: {
			const char *r = Q->value.as.cstr;

			// TODO: potential SQL injection? maybe later...
			fmt  = " %s.%s = '%s' AND";
			sz  += snprintf(PTR(), SIZE(), fmt, lt, lc, r);
			break;
		}
		case QME_LONG: {
			long r = Q->value.as.i64;

			fmt  = " %s.%s = '%ld' AND";
			sz  += snprintf(PTR(), SIZE(), fmt, lt, lc, r);
			break;
		}
		}
	}

	for (int i = 0; i < withinsz; ++i) {
		const struct QMwithin *Q = within[i];
		long ofs		 = Q->timeofs.offset * Q->timeofs.time;

		const char fmt[] = " unixepoch() - %s.%s <= %ld AND";

		sz += snprintf(PTR(), SIZE(), fmt, Q->table, Q->column, ofs);
	}

	for (int i = 0; i < betweensz; ++i) {
		const struct QMbetween *Q   = between[i];
		const char	       *t   = Q->table;
		const char	       *c   = Q->column;
		long			min = Q->min;
		long			max = Q->max;

		const char fmt[] = " %s.%s > %ld AND %s.%s < %ld AND";

		sz += snprintf(PTR(), SIZE(), fmt, t, c, min, t, c, max);
	}

	if (buf) {
		buf[sz - 4] = ';';
		buf[sz - 3] = '\0';
	}

	sc_free(&bump);

	return sz;

OUT_OF_MEMORY:
	sc_free(&bump);

	if (buf)
		*buf = '\0';

	return 0;

#undef SIZE
#undef PTR
}

static inline int q_match(struct sqlite3 *db,
			  char		**errmsg,
			  q_clbk	 *callback,
			  void		 *data,
			  struct Qmatch	  Q)
{
	int	      ret = 0;
	struct sc_mem buf = { .buf = NULL };

	int		qlen  = Q.qlen;
	struct QMbase **qs    = Q.qlist;
	struct jy_defs *names = Q.names;

	size_t		      arsz  = sizeof(void *) * qlen;
	const struct QMjoin **joins = sc_alloc(&buf, arsz);

	if (joins == NULL)
		goto OUT_OF_MEMORY;

	const struct QMbinary **binary = sc_alloc(&buf, arsz);

	if (binary == NULL)
		goto OUT_OF_MEMORY;

	const struct QMbetween **between = sc_alloc(&buf, arsz);

	if (between == NULL)
		goto OUT_OF_MEMORY;

	const struct QMwithin **within = sc_alloc(&buf, arsz);

	if (within == NULL)
		goto OUT_OF_MEMORY;

	const struct jy_defs **events = sc_alloc(&buf, arsz * 2);

	if (events == NULL)
		goto OUT_OF_MEMORY;

	const char **eventnames = sc_alloc(&buf, arsz * 2);

	if (eventnames == NULL)
		goto OUT_OF_MEMORY;

	int eventsz   = 0;
	int joinsz    = 0;
	int binsz     = 0;
	int withinsz  = 0;
	int betweensz = 0;

	for (int i = 0; i < qlen; ++i) {
		const struct QMbase *Q = qs[i];

		union jy_value event;
		enum jy_ktype  type;

		if (def_get(names, Q->table, &event, &type))
			goto INV_QUERY;

		if (type != JY_K_EVENT)
			goto INV_QUERY;

		if (!exists(eventsz, events, event.def)) {
			events[eventsz]	     = event.def;
			eventnames[eventsz]  = Q->table;
			eventsz		    += 1;
		}

		switch (Q->type) {
		case QM_NONE:
			assert(Q->type != QM_NONE);
			break;
		case QM_BINARY:
			binary[binsz]  = (struct QMbinary *) Q;
			binsz	      += 1;
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

		if (def_get(names, J->tbl_right, &event, &type))
			goto INV_QUERY;

		if (type != JY_K_EVENT)
			goto INV_QUERY;

		if (!exists(eventsz, events, event.def)) {
			events[eventsz]	     = event.def;
			eventnames[eventsz]  = J->tbl_right;
			eventsz		    += 1;
		}
	}

	int sz = prslcq(0, NULL, eventsz, joinsz, binsz, withinsz, betweensz,
			events, eventnames, joins, binary, between, within);

	if (sz == 0)
		goto OUT_OF_MEMORY;

	char *sql = sc_alloc(&buf, sz);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	prslcq(sz, sql, eventsz, joinsz, binsz, withinsz, betweensz, events,
	       eventnames, joins, binary, between, within);

	// This shouldn't happen, but just to make sure...
	if (*sql == '\0')
		goto OUT_OF_MEMORY;

	if (sqlite3_exec(db, sql, callback, data, errmsg) != SQLITE_OK)
		goto INV_QUERY;

	goto FINISH;

OUT_OF_MEMORY:
	ret = 1;
	goto FINISH;

INV_QUERY:
	ret = 2;

FINISH:
	sc_free(&buf);
	return ret;
}

#endif // JAYVM_Q_H
