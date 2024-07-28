#ifndef tvm_fnv_h
#define tvm_fnv_h

#include <stddef.h>
#include <stdint.h>

uint32_t fnv_hash(const char* str, size_t length);

#endif