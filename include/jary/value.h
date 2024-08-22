#ifndef JAYVM_VALUE_H
#define JAYVM_VALUE_H

#include <stdint.h>

typedef uint64_t Value;

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)
#define TAG_NIL 1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE 3 // 11.

#define VAL_FALSE ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define VAL_TRUE ((Value)(uint64_t)(QNAN | TAG_TRUE))

#endif // JAYVM_VALUE_H