#ifndef JAYVM_SCAN_H
#define JAYVM_SCAN_H

#include "token.h"

#include <stddef.h>

// line and ofs start from 1
void jry_scan(const char  *src,
	      size_t	   length,
	      enum jy_tkn *type,
	      size_t	  *line,
	      size_t	  *ofs,
	      const char **lxstart,
	      const char **lxend);

#endif // JAYVM_SCAN_H

