#ifndef TVM_COMMON_H
#define TVM_COMMON_H

#if defined(_MSC_VER)
    // Microsoft Visual Studio
    #define PACKED_STRUCT(name) \
        __pragma(pack(push, 1)) struct name __pragma(pack(pop))

#elif defined(__GNUC__) || defined(__clang__) || defined(__ICC)
    // GCC or Clang
    #define PACKED_STRUCT(name) struct __attribute__((packed)) name

#else
    // Default fallback
    #define PACKED_STRUCT(name) struct name
#endif // PACKED_STRUCT

#endif