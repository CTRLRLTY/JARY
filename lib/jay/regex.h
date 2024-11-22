#include "jary/memory.h"

#include <stdint.h>

typedef uint16_t rgxast_t;

enum {
	RGX_OK	  = 0,
	RGX_ERROR = 4,
};

enum RGXAST {
	RGXAST_ROOT,
	RGXAST_ESCAPE,
	RGXAST_SINGLE,
	RGXAST_CHARSET,
	RGXAST_OR,
	RGXAST_STAR,
	RGXAST_PLUS,
	RGXAST_QMARK,
	RGXAST_CONCAT,
};

struct rgxast {
	enum RGXAST *type;
	char	    *c;
	rgxast_t   **child;
	rgxast_t    *childsz;
	rgxast_t     size;
};

int rgx_parse(struct sc_mem *alloc,
	      const char    *pattern,
	      struct rgxast *list,
	      const char   **errmsg);
