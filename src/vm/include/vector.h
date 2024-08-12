#ifndef JAYVM_VECTOR_H
#define JAYVM_VECTOR_H

#include "memory.h"
#include "error.h"

#define VEC_GROW_NUMBER 10

typedef struct jary_vec_metadata_t {
    size_t count;
    size_t capacity;
} jary_vec_metadata_t;

#define jary_vec_t(__type) __type *
#define jary_vec_metadata(__vec) (&((jary_vec_metadata_t*)(__vec))[-1])
#define jary_vec_init(__vec, __capacity)                                                                             \
    do {                                                                                                            \
        jary_vec_metadata_t* m = jary_alloc(sizeof(jary_vec_metadata_t) + sizeof(*(__vec)) * (__capacity));         \
        (__vec) = (void*)&m[1];                                                                                     \
        jary_vec_metadata(__vec)->count = 0;                                                                        \
        jary_vec_metadata(__vec)->capacity = (__capacity);                                                          \
    } while(0)

#define jary_vec_size(__vec) jary_vec_metadata(__vec)->count
#define jary_vec_capacity(__vec) jary_vec_metadata(__vec)->capacity

#define jary_vec_grow(__vec, __capacity)                                                                \
    do {                                                                                                \
        jary_assert((__vec) != NULL);                                                                   \
        jary_vec_metadata_t* m = jary_vec_metadata((__vec));                                            \
        m = jary_realloc(m, sizeof(jary_vec_metadata_t) + sizeof(*(__vec)) * (__capacity));             \
        jary_assert(m != NULL);                                                                         \
        m->capacity = __capacity;                                                                       \
        (__vec) = (void*)&m[1];                                                                         \
    } while(0)

#define jary_vec_push(__vec, __data)                                                                   \
    do {                                                                                               \
        jary_assert((__vec) != NULL);                                                                  \
        if (jary_vec_size(__vec) + 1 >= jary_vec_capacity((__vec))) {                                  \
            jary_vec_grow(__vec, jary_vec_capacity(__vec) + VEC_GROW_NUMBER);                          \
        }                                                                                              \
        (__vec)[jary_vec_size(__vec)] = __data;                                                        \
        jary_vec_metadata((__vec))->count++;                                                           \
    } while(0)

#define jary_vec_free(__vec)                                                                           \
    do {                                                                                               \
        jary_free(jary_vec_metadata((__vec)));                                                         \
        (__vec) = NULL;                                                                                \
    } while(0)

#endif // JAYVM_VECTOR_H