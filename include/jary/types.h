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

#ifndef JAYVM_TYPES_H
#define JAYVM_TYPES_H

#include <stdint.h>

enum jy_ktype {
	JY_K_UNKNOWN = 0,
	JY_K_RULE,
	JY_K_INGRESS,

	JY_K_MODULE,
	JY_K_DESCRIPTOR,

	JY_K_FUNC,
	JY_K_ACTION,
	JY_K_MATCH,

	JY_K_EVENT,

	JY_K_REGEX,
	JY_K_TIME,
	JY_K_LONG,
	JY_K_ULONG,
	JY_K_OFS = JY_K_ULONG,
	JY_K_STR,
	JY_K_BOOL,

	JY_K_HANDLE,
};

struct jy_desc {
	uint32_t name;
	uint32_t member;
};

struct jy_time_ofs {
	enum {
		JY_TIME_HOUR   = 3600,
		JY_TIME_MINUTE = 60,
		JY_TIME_SECOND = 1,
	} time;

	int offset;
};

// generic view for all qualified Jary values
union jy_value {
	void		  *handle;
	struct jy_func	  *func;
	struct jy_str	  *str;
	char		  *cstr;
	struct jy_defs	  *def;
	struct jy_defs	  *module;
	struct jy_desc	   dscptr;
	long		   i64;
	unsigned long	   u64;
	unsigned long	   ofs;
	struct jy_time_ofs timeofs;
};

struct jy_str {
	// size does not include '\0'
	uint32_t size;
	// null terminated string
	char	 cstr[];
};

struct jy_state;

struct jy_func {
	enum jy_ktype return_type;
	uint8_t	      param_size;
	int (*func)(struct jy_state *, int, union jy_value *, union jy_value *);
	enum jy_ktype param_types[];
};

#ifndef __cplusplus
// Just for clarity
_Static_assert(sizeof(union jy_value) == 8, "Values must be 8 bytes");
#endif // __cplusplus

#endif // JAYVM_TYPES_H
