#ifndef JAYVM_COMMON_H
#define JAYVM_COMMON_H

#ifdef __GNUC__
#	define UNUSED(x)  UNUSED_##x __attribute__((__unused__))
#	define USE_RESULT __attribute__((warn_unused_result))
#else
#	define UNUSED(x) UNUSED_##x
#	define USE_RESULT
#endif // __GNUC__

#endif // JAYVM_COMMON_H