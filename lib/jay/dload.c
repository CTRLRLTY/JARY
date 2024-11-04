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

#ifdef __GNUC__
#	define JAY_API __attribute__((visibility("default")))
#else
#	define JAY_API
#endif // __GNUC__

#include "dload.h"

#include "jary/defs.h"
#include "jary/memory.h"
#include "jary/modules.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct jy_module {
	struct jy_defs *def;
	const char     *errmsg;
};

typedef int (*loadfn_t)(struct jy_module *, const char **errmsg);
typedef int (*unloadfn_t)(struct jy_module *, const char **errmsg);

#ifdef __unix__
#	include <dlfcn.h>

int jry_dlload(const char *path, struct jy_defs *def, const char **msg)
{
	int	 ret = 0;
	loadfn_t load;

	int  pathsz   = strlen(path);
	char suffix[] = ".so";
	char modulepath[pathsz + sizeof(suffix)];

	strcpy(modulepath, path);
	strcat(modulepath, suffix);

	union jy_value handle;

	// TODO: use custom namespace
	handle.handle = dlopen(modulepath, RTLD_LAZY);

	if (handle.handle == NULL)
		goto DLOAD_ERROR;

	load = (loadfn_t) (uintptr_t) dlsym(handle.handle, "module_load");

	if (load == NULL)
		goto DLOAD_ERROR;

	const char k[] = "__handle__";

	if (def_add(def, k, handle, JY_K_HANDLE) != 0)
		goto OUT_OF_MEMORY;

	struct jy_module ctx = { .def = def, .errmsg = "not an error" };

	if (load(&ctx, msg) != JAY_OK)
		goto LOAD_FAIL;

	goto FINISH;

OUT_OF_MEMORY:
	ret  = 1;
	*msg = "out of memory";
	goto FINISH;

LOAD_FAIL:
	ret = 2;
	goto FINISH;

DLOAD_ERROR:
	*msg = dlerror();
	ret  = 3;

FINISH:
	return ret;
}

int jry_dlunload(struct jy_defs *def, const char **errmsg)
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
	status		     = unload(&ctx, errmsg);

CLOSE: {
	dlclose(handle.handle);
}
FINISH:
	return status;
}

#endif // __unix__

// > user modules API
JAY_API int jay_def_func(struct jy_module    *ctx,
			 const char	     *key,
			 enum jy_ktype	      return_type,
			 uint8_t	      param_size,
			 const enum jy_ktype *param_types,
			 int (*func)(struct jy_state *,
				     int,
				     union jy_value *,
				     union jy_value *))
{
	int	       ret = JAY_OK;
	union jy_value v;

	size_t parambytes = sizeof(*param_types) * param_size;
	size_t bytes	  = sizeof(*v.func) + parambytes;
	v.func		  = jry_alloc(bytes);

	if (v.func == NULL)
		goto OUT_OF_MEMORY;

	v.func->return_type = return_type;
	v.func->param_size  = param_size;
	v.func->func	    = func;

	memcpy(v.func->param_types, param_types, parambytes);

	if (def_add(ctx->def, key, v, JY_K_FUNC) != 0)
		goto OUT_OF_MEMORY;

	ctx->errmsg = "not an error";
	goto FINISH;

OUT_OF_MEMORY:
	ctx->errmsg = "out of memory";
	ret	    = JAY_ERR_OOM;
	goto FINISH;

FINISH:
	return ret;
}

JAY_API int jay_del_func(struct jy_module *ctx, const char *key)
{
	int	 ret = JAY_OK;
	uint32_t id;

	if (!def_find(ctx->def, key, &id))
		goto OUT_OF_MEMORY;

	if (ctx->def->types[id] != JY_K_FUNC)
		goto INV_FUNC;

	jry_free(ctx->def->vals[id].func);

	ctx->errmsg = "not an error";
	goto FINISH;

INV_FUNC:
	ctx->errmsg = "not function";
	ret	    = JAY_ERR_MISMATCH;
	goto FINISH;

OUT_OF_MEMORY:
	ctx->errmsg = "out of memory";
	ret	    = JAY_ERR_OOM;
	goto FINISH;

FINISH:
	return ret;
}

JAY_API const char *jay_errmsg(struct jy_module *ctx)
{
	return ctx->errmsg;
}

// < user modules API
