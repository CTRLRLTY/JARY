#ifndef tvm_vector_h
#define tvm_vector_h

#include <stddef.h>

#define VEC_INITIAL_CAPACITY 10

typedef enum {
    VEC_SUCCESS = 0,
    VEC_FOUND = 0,
    VEC_NOT_FOUND,
    ERR_VEC_NULLPTR,
    ERR_VEC_DOUBLE_FREE,
    ERR_VEC_ALLOC_FAIL,
    ERR_VEC_EMPTY,

    // trying to fetch out of bound index
    ERR_VEC_OUT_OF_BOUND,
    ERR_VEC_INVARIANT,
} VectorError;

typedef struct {
    // total element in the vector
    size_t count;  
    // maximum capacity before grow is required    
    size_t capacity; 
    // the size of the base element   
    size_t size;      

    void* data;
} Vector;

// must call vec_free for initialized Vector
VectorError vec_init(Vector* vec, size_t datalen);
VectorError vec_get(Vector* vec, size_t index, void* data, size_t datalen);
VectorError vec_push(Vector* vec, void* data, size_t datalen);
VectorError vec_pop(Vector* vec, void* data, size_t datalen);
VectorError vec_free(Vector* vec);
VectorError vec_find(Vector* vec, void* data, size_t datalen, void* buf, size_t buflen);

#endif