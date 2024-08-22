#ifndef JAYVM_SCAN_H
#define JAYVM_SCAN_H

#include "token.h"

#include <stddef.h>

// line and ofs start from 1
size_t ji_scan(const char *src, size_t length, jy_tkn_type_t *type,
	       size_t *line, size_t *ofs, char **lexeme, size_t *lexsz);

#endif // JAYVM_SCAN_H