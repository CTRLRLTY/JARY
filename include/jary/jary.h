#ifndef JARY_H
#define JARY_H

#include <stdlib.h>
#define JARY_OK		 0
// generic error, which need to be updated later!
#define JARY_ERROR	 0x2
#define JARY_ERR_OOM	 0x4
#define JARY_ERR_COMPILE 0x10
#define JARY_ERR_SQLITE3 0x20

struct jary;

int jary_open(struct jary **jary);
int jary_close(struct jary *restrict jary);

int jary_write_event(struct jary *restrict jary,
		     const char	 *name,
		     int	  length,
		     const char **values);

int jary_compile(struct jary *restrict jary,
		 size_t	     size,
		 const char *source,
		 const char *modulepath);

int jary_execute(struct jary *restrict jary);

#endif
