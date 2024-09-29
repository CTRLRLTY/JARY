#include "dload.h"

#include "jary/defs.h"
#include "jary/memory.h"
#include "jary/object.h"

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
	[MSG_DYNAMIC]	 = NULL,
};

struct jy_module {
	struct jy_defs		   *def;
	struct jy_object_allocator *obj;
};

typedef int (*loadfn_t)(struct jy_module *);
typedef int (*unloadfn_t)(struct jy_module *);

#ifdef __unix__
#	include <dlfcn.h>

int jry_module_load(const char		       *path,
		    struct jy_defs	       *def,
		    struct jy_object_allocator *object)
{
	loadfn_t load;

	int  pathsz   = strlen(path);
	char suffix[] = ".so";
	char modulepath[pathsz + sizeof(suffix)];

	strcpy(modulepath, path);
	strcat(modulepath, suffix);

	union {
		jy_val_t value;
		void	*ptr;
	} handle;

	handle.ptr = dlopen(modulepath, RTLD_LAZY);

	if (handle.ptr == NULL)
		goto DLOAD_ERROR;

	load = (loadfn_t) (uintptr_t) dlsym(handle.ptr, "module_load");

	if (load == NULL)
		goto DLOAD_ERROR;

	const char k[] = "__handle__";

	if (jry_add_def(def, k, handle.value, JY_K_HANDLE) != 0)
		goto DLOAD_ERROR;

	struct jy_module ctx	= { .def = def, .obj = object };
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

	union {
		jy_val_t value;
		void	*ptr;
	} handle;

	const char k[] = "__handle__";
	uint32_t   nid;

	assert(jry_find_def(def, k, &nid));

	handle.value = def->vals[nid];

	if (handle.ptr == NULL)
		goto FINISH;

	unloadfn_t unload;

	unload = (unloadfn_t) (uintptr_t) dlsym(handle.ptr, "module_unload");

	if (unload == NULL)
		goto CLOSE;

	struct jy_module ctx = { .def = def, .obj = NULL };
	status		     = unload(&ctx);

CLOSE: {
	dlclose(handle.ptr);
	jry_free_def(*def);
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
int define_function(struct jy_module	*ctx,
		    const char		*key,
		    enum jy_ktype	 return_type,
		    uint8_t		 param_size,
		    const enum jy_ktype *param_types,
		    jy_funcptr_t	 func)
{
	struct jy_obj_func *ofunc = NULL;
	uint32_t allocsz = sizeof(*ofunc) + sizeof(*param_types) * param_size;
	ofunc		 = jry_allocobj(allocsz, allocsz, ctx->obj);

	if (ofunc == NULL)
		return TO_ERROR(MSG_UNKNOWN);

	ofunc->return_type = return_type;
	ofunc->param_size  = param_size;
	ofunc->param_types = (void *) (ofunc + 1);
	ofunc->func	   = func;

	memcpy(ofunc->param_types, param_types,
	       sizeof(*param_types) * param_size);

	jy_val_t value = jry_long2v(memory_offset(ctx->obj->buf, ofunc));

	if (jry_add_def(ctx->def, key, value, JY_K_FUNC) != 0)
		return TO_ERROR(MSG_UNKNOWN);

	return 0;
}

const char *error_message(int status)
{
	return jry_module_error(status);
}

// < user modules API
