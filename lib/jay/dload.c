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

#include "dload.h"

#include "jary/defs.h"
#include "jary/memory.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_MSG_COUNT_LIMIT 128

#define TO_INDEX(__status)    ((0x7F000000 & (__status)) >> 24)
#define TO_ERROR(__status)    (((__status) << 24) | 0x80000000)

#define MSG_UNKNOWN	      0
#define MSG_INV_MODULE	      1
#define MSG_LOAD_FAIL	      2
#define MSG_DYNAMIC	      (ERROR_MSG_COUNT_LIMIT - 1)

static const char *error_reason[ERROR_MSG_COUNT_LIMIT] = {
	[MSG_UNKNOWN]	 = "unknown",
	[MSG_INV_MODULE] = "module does not exist",
	[MSG_LOAD_FAIL]	 = "load failed",

	// reserved for dynamic error
	[MSG_DYNAMIC] = NULL,
};

struct jy_module {
	struct jy_defs *def;
};

typedef int (*loadfn_t)(struct jy_module *);
typedef int (*unloadfn_t)(struct jy_module *);

#ifdef __unix__
#	include <dlfcn.h>

int jry_module_load(const char *path, struct jy_defs *def)
{
	loadfn_t load;

	int  pathsz   = strlen(path);
	char suffix[] = ".so";
	char modulepath[pathsz + sizeof(suffix)];

	strcpy(modulepath, path);
	strcat(modulepath, suffix);

	union jy_value handle;

	handle.handle = dlopen(modulepath, RTLD_LAZY);

	if (handle.handle == NULL)
		goto DLOAD_ERROR;

	load = (loadfn_t) (uintptr_t) dlsym(handle.handle, "module_load");

	if (load == NULL)
		goto DLOAD_ERROR;

	const char k[] = "__handle__";

	if (def_add(def, k, handle, JY_K_HANDLE) != 0)
		goto DLOAD_ERROR;

	struct jy_module ctx	= { .def = def };
	int		 status = load(&ctx);

	if (status != 0)
		return TO_ERROR(MSG_LOAD_FAIL);

	return 0;

DLOAD_ERROR:
	error_reason[MSG_DYNAMIC] = dlerror();
	return TO_ERROR(MSG_DYNAMIC);
}

int jry_module_unload(struct jy_defs *def)
{
	int status = 0;

	union jy_value handle;

	const char k[] = "__handle__";
	uint32_t   nid;

	if (!def_find(def, k, &nid))
		goto FINISH;

	handle = def->vals[nid];

	if (handle.handle == NULL)
		goto FINISH;

	unloadfn_t unload;

	unload = (unloadfn_t) (uintptr_t) dlsym(handle.handle, "module_unload");

	if (unload == NULL)
		goto CLOSE;

	struct jy_module ctx = { .def = def };
	status		     = unload(&ctx);

CLOSE: {
	dlclose(handle.handle);
}
FINISH:
	return status;
}

const char *jry_module_error(int errcode)
{
	int	    err = TO_INDEX(errcode);
	const char *msg = error_reason[err];

	if (msg == NULL)
		return error_reason[MSG_UNKNOWN];

	return msg;
}

#endif // __unix__

// > user modules API
int def_func(struct jy_module	 *ctx,
	     const char		 *key,
	     enum jy_ktype	  return_type,
	     uint8_t		  param_size,
	     const enum jy_ktype *param_types,
	     jy_funcptr_t	  func)
{
	union jy_value v;

	size_t parambytes = sizeof(*param_types) * param_size;
	size_t bytes	  = sizeof(*v.func) + parambytes;
	v.func		  = jry_alloc(bytes);

	if (v.func == NULL)
		return TO_ERROR(MSG_UNKNOWN);

	v.func->return_type = return_type;
	v.func->param_size  = param_size;
	v.func->func	    = func;

	memcpy(v.func->param_types, param_types, parambytes);

	if (def_add(ctx->def, key, v, JY_K_FUNC) != 0)
		return TO_ERROR(MSG_UNKNOWN);

	return 0;
}

int del_func(struct jy_module *ctx, const char *key)
{
	uint32_t id;

	if (!def_find(ctx->def, key, &id))
		return TO_ERROR(MSG_UNKNOWN);

	if (ctx->def->types[id] != JY_K_FUNC)
		return TO_ERROR(MSG_UNKNOWN);

	jry_free(ctx->def->vals[id].func);

	return 0;
}

const char *error_message(int status)
{
	return jry_module_error(status);
}

// < user modules API
