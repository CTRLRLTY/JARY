#ifndef JAYVM_SCAN_H
#define JAYVM_SCAN_H

#include "token.h"

#include <stdint.h>

const char *jry_scan(const char *start, uint32_t length, enum jy_tkn *type);

#endif // JAYVM_SCAN_H
