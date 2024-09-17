#ifndef JAYVM_SCAN_H
#define JAYVM_SCAN_H

#include "token.h"

#include <stdint.h>

// line and ofs start from 1
void jry_scan(const char  *src,
	      uint32_t	   length,
	      enum jy_tkn *type,
	      uint32_t	  *line,
	      uint32_t	  *ofs,
	      const char **lxstart,
	      const char **lxend);

#endif // JAYVM_SCAN_H
