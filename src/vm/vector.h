#ifndef JAYVM_VECTOR_H
#define JAYVM_VECTOR_H

#include <string.h>

#include "memory.h"
#include "error.h"

#define ARR_GROW_RATE 10

typedef struct vecmeta_t {
    size_t count;
    size_t capacity;
} vecmeta_t;

#define vec_t(__type) __type *
#define vecmetadata(__vec) (&((vecmeta_t*)(__vec))[-1])
#define vecinit(__vec, __capacity)                                                                                  \
    do {                                                                                                            \
        vecmeta_t* __m = jary_alloc(sizeof(vecmeta_t) + sizeof(*(__vec)) * (__capacity));                           \
        (__vec) = (void*)&__m[1];                                                                                   \
        vecmetadata((__vec))->count = 0;                                                                            \
        vecmetadata((__vec))->capacity = (__capacity);                                                              \
    } while(0)

#define vecsize(__vec) ((__vec) ? vecmetadata((__vec))->count : 0)
#define veccap(__vec) ((__vec) ? vecmetadata((__vec))->capacity : 0)

#define vecgrow(__vec, __capacity)                                                                      \
    do {                                                                                                \
        jary_assert((__vec) != NULL);                                                                   \
        vecmeta_t* __m = vecmetadata((__vec));                                                          \
        __m = jary_realloc(__m, sizeof(vecmeta_t) + sizeof(*(__vec)) * (__capacity));                   \
        jary_assert(__m != NULL);                                                                       \
        __m->capacity = __capacity;                                                                     \
        (__vec) = (void*)&__m[1];                                                                       \
    } while(0)

#define vecpush(__vec, __data)                                                                         \
    do {                                                                                               \
        jary_assert((__vec) != NULL);                                                                  \
        if (vecsize((__vec)) + 1 >= veccap((__vec))) {                                                 \
            vecgrow((__vec), veccap((__vec)) + ARR_GROW_RATE);                                         \
        }                                                                                              \
        (__vec)[vecsize((__vec))] = (__data);                                                          \
        vecmetadata((__vec))->count = vecsize((__vec)) + 1;                                            \
    } while(0)

#define vecpop(__vec) (&(__vec)[--vecmetadata((__vec))->count])

#define vecfree(__vec)                                                                                 \
    do {                                                                                               \
        jary_free(vecmetadata((__vec)));                                                               \
        (__vec) = NULL;                                                                                \
    } while(0)

#define veclast(__vec) (&(__vec)[vecsize((__vec)) ? vecsize((__vec)) - 1 : 0])

#define vecset(__vec, __char, __nbytes)                                                               \
        do {                                                                                          \
            if (vecsize((__vec)) + (__nbytes) >= veccap((__vec)))                                     \
                vecgrow((__vec), veccap((__vec)) + (__nbytes) + ARR_GROW_RATE);                       \
            memset(&(__vec)[vecsize((__vec))], (__char), (__nbytes));                                 \
            vecmetadata((__vec))->count += (__nbytes);                                                \
        } while(0)

#define veccat(__vec, __src, __nbytes)                                                                \
        do {                                                                                          \
            if (vecsize((__vec)) + (__nbytes) >= veccap((__vec)))                                     \
                vecgrow((__vec), veccap((__vec)) + (__nbytes) + ARR_GROW_RATE);                       \
            memcpy(&(__vec)[vecsize((__vec))], (__src), (__nbytes));                                  \
            vecmetadata((__vec))->count += (__nbytes);                                                \
        } while(0)

#endif // JAYVM_VECTOR_H