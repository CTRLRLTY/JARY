#ifndef JAYVM_FNV_H
#define JAYVM_FNV_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t strhash_t;

strhash_t jry_strhash(const char *str, size_t length);

#endif // JAYVM_FNV_H