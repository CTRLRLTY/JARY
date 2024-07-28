#include <stdlib.h>
#include <string.h>

#include "vector.h"

VectorError vec_init(Vector* vec, size_t size) {
    if (vec == NULL)
        return ERR_VEC_NULLPTR;

    vec->count = 0;
    vec->capacity = VEC_INITIAL_CAPACITY;
    vec->size = size;
    vec->data = calloc(vec->capacity, vec->size);

    if (vec->data == NULL)
        return ERR_VEC_ALLOC_FAIL;

    return VEC_SUCCESS;
}

VectorError vec_free(Vector* vec) {
    if (vec == NULL)
        return ERR_VEC_NULLPTR;

    if (vec->data == NULL)
        return ERR_VEC_DOUBLE_FREE;

    vec->count = 0;
    vec->capacity = 0;
    vec->size = 0;
    free(vec->data);
    vec->data = NULL;

    return VEC_SUCCESS;
}

VectorError vec_get(Vector* vec, size_t index, void* buf, size_t buflen) {
    if (vec == NULL || buf == NULL)
        return ERR_VEC_NULLPTR;

    if (buflen != vec->size)
        return ERR_VEC_INVARIANT;

    if (index >= vec->count)
        return ERR_VEC_OUT_OF_BOUND;

    void* src = (char*)vec->data + (index * vec->size);

    memcpy(buf, src, vec->size);

    return VEC_SUCCESS;
}

VectorError vec_push(Vector* vec, void* data, size_t datalen) {
    if (vec == NULL || data == NULL)
        return ERR_VEC_NULLPTR;

    if (datalen != vec->size)
        return ERR_VEC_INVARIANT;

    if (vec->count+1 == vec->capacity) {
        vec->capacity += VEC_INITIAL_CAPACITY;
        vec->data = realloc(vec->data, vec->capacity * vec->size);

        if (vec->data == NULL) 
            return ERR_VEC_ALLOC_FAIL;
    }

    void* dst = (char*)vec->data + (vec->count * vec->size);

    memcpy(dst, data, vec->size);

    ++vec->count;

    return VEC_SUCCESS;
}

VectorError vec_pop(Vector* vec, void* data, size_t datalen) {
    if (vec == NULL)
        return ERR_VEC_NULLPTR;

    if (datalen != vec->size && data != NULL)
        return ERR_VEC_INVARIANT;

    if (vec->count == 0) 
        return ERR_VEC_EMPTY;

    --vec->count;

    if (data != NULL) {
        void* src = (char*)vec->data + (vec->count * vec->size);
        memcpy(data, src, vec->size);
    }

    return VEC_SUCCESS;
}

VectorError vec_find(Vector* vec, void* data, size_t datalen, void* buf, size_t buflen) {
    if (vec == NULL || data == NULL)
        return ERR_VEC_NULLPTR;

    if (datalen != vec->size)
        return ERR_VEC_INVARIANT;
    
    if (buflen != vec->size && buf != NULL)
        return ERR_VEC_INVARIANT;
    
    for (size_t i = 0; i < vec->count; ++i) {
        void* src = (char*)vec->data + (i * vec->size);
        
        if (memcmp(src, data, vec->size) != 0)
            continue; 

        if (buf != NULL) 
            memcpy(buf, src, vec->size);
        
        return VEC_FOUND;
    }

    return VEC_NOT_FOUND;
}