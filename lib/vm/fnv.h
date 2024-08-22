#ifndef JAYVM_FNV_H
#define JAYVM_FNV_H

#include <stddef.h>
#include <stdint.h>

uint32_t fnv_hash(const char *str, size_t length);

#endif // JAYVM_FNV_H