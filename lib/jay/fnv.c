#include "jary/fnv.h"

strhash_t jry_strhash(const char *str, size_t length)
{
	uint32_t hash = 2166136261u;
	for (size_t i = 0; i < length; ++i) {
		hash ^= (uint8_t) str[i];
		hash *= 16777619;
	}

	return hash;
}