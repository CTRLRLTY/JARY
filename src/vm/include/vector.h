#ifndef JAYVM_VECTOR_H
#define JAYVM_VECTOR_H

#include "memory.h"
#include "error.h"

#define VEC_GROW_NUMBER 10

typedef struct jary_vec_metadata_t {
    size_t count;
    size_t capacity;
} jary_vec_metadata_t;

#define jary_vec_t(type) type *
#define jary_vec_metadata(vec) (&((jary_vec_metadata_t*)(vec))[-1])
#define jary_vec_init(vec, _capacity)                                                                           \
    do {                                                                                                        \
        jary_vec_metadata_t* m = jary_alloc(sizeof(jary_vec_metadata_t) + sizeof(*(vec)) * (_capacity));        \
        (vec) = (void*)&m[1];                                                                                   \
        jary_vec_metadata(vec)->count = 0;                                                                      \
        jary_vec_metadata(vec)->capacity = (_capacity);                                                         \
    } while(0)

#define jary_vec_size(vec) jary_vec_metadata(vec)->count
#define jary_vec_capacity(vec) jary_vec_metadata(vec)->capacity

#define jary_vec_grow(vec, _capacity)                                                               \
    do {                                                                                            \
        jary_assert((vec) != NULL);                                                                 \
        jary_vec_metadata_t* m = jary_vec_metadata((vec));                                          \
        m = jary_realloc(m, sizeof(jary_vec_metadata_t) + sizeof(*(vec)) * (_capacity));            \
        jary_assert(m != NULL);                                                                     \
        (vec) = (void*)&m[1];                                                                       \
    } while(0)

#define jary_vec_push(vec, data)                                                            \
    do {                                                                                    \
        jary_assert((vec) != NULL);                                                         \
        if (jary_vec_size(vec) + 1 >= jary_vec_capacity((vec))) {                           \
            jary_vec_grow(vec, jary_vec_capacity(vec) + VEC_GROW_NUMBER);                   \
        }                                                                                   \
        (vec)[jary_vec_size(vec)] = data;                                                   \
        jary_vec_metadata((vec))->count++;                                                  \
    } while(0)

#define jary_vec_free(vec)                                                                  \
    do {                                                                                    \
        jary_free(jary_vec_metadata((vec)));                                                \
        vec = NULL;                                                                         \
    } while(0)

#endif // JAYVM_VECTOR_H