// THESE ARE SUCH A BAD IMPLEMENTATION, BUT IM STRAPPED FOR TIME!!!

#ifndef JAYVM_Q_H
#define JAYVM_Q_H

#define Q_COL_INT 0x1
#define Q_COL_STR 0x2

typedef int (*q_callback_t)(void *, int, char **, char **);

struct sqlite3;

enum QMtag {
	QM_EXACT,
	QM_JOIN,
};

struct QMbase {
	enum QMtag  type;
	const char *table;
	const char *column;
};

struct QMexact {
	enum QMtag  type;
	const char *table;
	const char *column;
	const char *value;
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
};

struct Qcreate {
	int	     colsz;
	const char  *table;
	const char **columns;
	int	    *flags;
};

struct Qinsert {
	int	     colsz;
	const char  *table;
	const char **columns;
	const char **values;
};

int q_match(struct sqlite3 *db,
	    char	  **errmsg,
	    q_callback_t    cb,
	    void	   *data,
	    struct Qmatch   Q);
int q_create(struct sqlite3 *db, struct Qcreate Q);
int q_insert(struct sqlite3 *db, struct Qinsert Q);

#endif // JAYVM_Q_H
