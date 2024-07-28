#include "fnv.h"


uint32_t fnv_hash(const char* str, size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i)
    {
        hash ^= (uint8_t)str[i];
        hash *= 16777619; 
    }

    return hash;
}