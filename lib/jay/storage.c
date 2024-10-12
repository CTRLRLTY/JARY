#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int jry_sqlstr_crt_event(const char	     *name,
			 unsigned short	      colsz,
			 char *const	     *columns,
			 const enum jy_ktype *types,
			 char		    **sql)
{
	char *buf = NULL;
	asprintf(sql, "CREATE TABLE IF NOT EXISTS %s (", name);

	if (*sql == NULL)
		goto OUT_OF_MEMORY;

	buf = *sql;

	for (unsigned short i = 0; i < colsz; ++i) {
		const char *type = NULL;

		switch (types[i]) {
		case JY_K_LONG:
			type = "INTEGER";
			break;
		case JY_K_STR:
			type = "TEXT";
			break;
		default:
			continue;
		}

		const char *key = columns[i];

		if (strcmp(key, "__name__") == 0)
			continue;

		asprintf(sql, "%s%s %s,", buf, key, type);

		if (*sql == NULL)
			goto OUT_OF_MEMORY;

		free(buf);
		buf = *sql;
	}

	size_t len = strlen(buf);

	if (buf[len - 1] != ',')
		goto INVALID_TABLE;

	buf[len - 1] = ')';

	asprintf(sql, "%s;", buf);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	free(buf);
	return 0;

OUT_OF_MEMORY:
	free(buf);
	return 1;

INVALID_TABLE:
	free(buf);
	return 2;
}

int jry_sqlstr_ins_event(const char    *name,
			 unsigned short colsz,
			 char *const   *columns,
			 char	      **sql)
{
	char *buf = NULL;
	asprintf(sql, "INSERT INTO %s (", name);

	if (*sql == NULL)
		goto OUT_OF_MEMORY;

	buf = *sql;

	for (unsigned short i = 0; i < colsz; ++i) {
		const char *key = columns[i];

		if (key == NULL)
			continue;

		if (strcmp(key, "__name__") == 0)
			continue;

		asprintf(sql, "%s%s,", buf, key);

		if (*sql == NULL)
			goto OUT_OF_MEMORY;

		free(buf);
		buf = *sql;
	}

	size_t len = strlen(buf);

	if (buf[len - 1] != ',')
		goto INVALID_TABLE;

	buf[len - 1] = ')';

	asprintf(sql, "%s VALUES (", buf);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	free(buf);
	buf = *sql;

	for (unsigned short i = 0; i < colsz; ++i) {
		const char *key = columns[i];

		if (key == NULL)
			continue;

		asprintf(sql, "%s?,", buf);

		if (*sql == NULL)
			goto OUT_OF_MEMORY;

		free(buf);
		buf = *sql;
	}

	len	     = strlen(buf);
	buf[len - 1] = ')';

	asprintf(sql, "%s;", buf);

	if (sql == NULL)
		goto OUT_OF_MEMORY;

	free(buf);
	return 0;

OUT_OF_MEMORY:
	free(buf);
	return 1;

INVALID_TABLE:
	free(buf);
	return 2;
}
