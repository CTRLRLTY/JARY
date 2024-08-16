#ifndef JAYVM_VECTOR_H
#define JAYVM_VECTOR_H

#include "memory.h"
#include "error.h"

#define ARR_GROW_RATE 10

typedef struct jary_vec_metadata_t {
    size_t count;
    size_t capacity;
} jary_vec_metadata_t;

#define jary_vec_t(__type) __type *
#define jary_vec_metadata(__vec) (&((jary_vec_metadata_t*)(__vec))[-1])
#define jary_vec_init(__vec, __capacity)                                                                            \
    do {                                                                                                            \
        jary_vec_metadata_t* __m = jary_alloc(sizeof(jary_vec_metadata_t) + sizeof(*(__vec)) * (__capacity));       \
        (__vec) = (void*)&__m[1];                                                                                   \
        jary_vec_metadata((__vec))->count = 0;                                                                      \
        jary_vec_metadata((__vec))->capacity = (__capacity);                                                        \
    } while(0)

#define jary_vec_size(__vec) ((__vec) ? jary_vec_metadata((__vec))->count : 0)
#define jary_vec_capacity(__vec) ((__vec) ? jary_vec_metadata((__vec))->capacity : 0)

#define jary_vec_grow(__vec, __capacity)                                                                \
    do {                                                                                                \
        jary_assert((__vec) != NULL);                                                                   \
        jary_vec_metadata_t* __m = jary_vec_metadata((__vec));                                          \
        __m = jary_realloc(__m, sizeof(jary_vec_metadata_t) + sizeof(*(__vec)) * (__capacity));         \
        jary_assert(__m != NULL);                                                                       \
        __m->capacity = __capacity;                                                                     \
        (__vec) = (void*)&__m[1];                                                                       \
    } while(0)

#define jary_vec_push(__vec, __data)                                                                   \
    do {                                                                                               \
        jary_assert((__vec) != NULL);                                                                  \
        if (jary_vec_size((__vec)) + 1 >= jary_vec_capacity((__vec))) {                                \
            jary_vec_grow((__vec), jary_vec_capacity((__vec)) + ARR_GROW_RATE);                        \
        }                                                                                              \
        (__vec)[jary_vec_size((__vec))] = (__data);                                                    \
        jary_vec_metadata((__vec))->count++;                                                           \
    } while(0)

#define jary_vec_pop(__vec) (&(__vec)[jary_vec_metadata((__vec))->count--])

#define jary_vec_free(__vec)                                                                           \
    do {                                                                                               \
        jary_free(jary_vec_metadata((__vec)));                                                         \
        (__vec) = NULL;                                                                                \
    } while(0)

#define jary_vec_last(__vec) (&(__vec)[jary_vec_size((__vec)) - 1])


#define jary_arr_grow(__arr, __pcap, __nmemb)                                                          \
    do {                                                                                               \
        jary_assert((__arr) != NULL);                                                                  \
        (__arr) = jary_realloc(m,  (__nmemb));                                                         \
        jary_assert((__arr) != NULL);                                                                  \
        *(__pcap) = (__nmemb)/sizeof(*(__arr));                                                        \
    } while(0)


#define jary_arr_push(__arr, __psize, __pcap, __data)                                                  \
    do {                                                                                               \
        jary_assert((__arr) != NULL);                                                                  \
        if (*(__psize) + 1 >= *(__pcap)) {                                                             \
            jary_arr_grow((__arr), (__pcap), sizeof(*(__arr)) + ARR_GROW_RATE);                        \
        }                                                                                              \
        (__arr)[*(__psize)] = (__data);                                                                \
        *(__psize) += 1;                                                                               \
    } while(0)

#define jary_arr_last(__arr, __size) (&(__arr)[(__size) - 1])

#endif // JAYVM_VECTOR_H