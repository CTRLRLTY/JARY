#ifndef JAYVM_COMMON_H
#define JAYVM_COMMON_H

#ifdef __GNUC__
#	define __unused(x)  UNUSED_##x __attribute__((__unused__))
#	define __use_result __attribute__((warn_unused_result))
#else
#	define __unused(x) UNUSED_##x
#	define __use_result
#endif // __GNUC__

#endif // JAYVM_COMMON_H
