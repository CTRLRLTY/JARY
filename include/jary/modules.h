/*
BSD 3-Clause License

Copyright (c) 2024. Muhammad Raznan. All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef JAYVM_MODULES_H
#define JAYVM_MODULES_H

#include <stdint.h>

#ifndef JAY_API
#	define JAY_API
#endif

#define JAY_OK		 0x0
#define JAY_ERR_OOM	 0x101
#define JAY_ERR_MISMATCH 0x102

// Crash the runtime
#define JAY_INT_CRASH	 0x103

enum jy_ktype {
	JY_K_LONG   = 1,
	JY_K_ULONG  = 2,
	JY_K_STR    = 3,
	JY_K_BOOL   = 4,
	JY_K_ACTION = 5,
};

// generic view for all qualified Jary values
union jy_value {
	struct jy_str *str;
	long	       i64;
	unsigned long  u64;
};

struct jy_str {
	// size does not include '\0'
	uint32_t size;
	// null terminated string
	char	 cstr[];
};

struct jy_state;
struct jy_module;

JAY_API int jay_def_func(struct jy_module    *ctx,
			 const char	     *key,
			 enum jy_ktype	      return_type,
			 uint8_t	      param_size,
			 const enum jy_ktype *param_types,
			 int (*func)(struct jy_state *,
				     int,
				     union jy_value *,
				     union jy_value *));

JAY_API int jay_del_func(struct jy_module *ctx, const char *key);

JAY_API const char *jay_errmsg(struct jy_module *ctx);

int module_load(struct jy_module *ctx, const char **errmsg);

int module_unload(struct jy_module *ctx, const char **errmsg);

#endif // JAYVM_MODULES_H
