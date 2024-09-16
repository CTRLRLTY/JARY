#include "dload.h"

#include "jary/defs.h"
#include "jary/object.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef int (*loadfn_t)(int);
typedef int (*unloadfn_t)(int);

#define MODULE_COUNT_LIMIT    1000
#define ERROR_MSG_COUNT_LIMIT 128

#define TO_INDEX(__status)    ((0x7F000000 & (__status)) >> 24)
#define TO_ERROR(__status)    (((__status) << 24) | 0x80000000)

#define MSG_UNKNOWN	      0
#define MSG_INV_MODULE	      1
#define MSG_LOAD_FAIL	      2
#define MSG_MODULE_CAPPED     3
#define MSG_DYNAMIC	      (ERROR_MSG_COUNT_LIMIT - 1)

static const char *error_reason[ERROR_MSG_COUNT_LIMIT] = {
	[MSG_UNKNOWN]	    = "unknown",
	[MSG_INV_MODULE]    = "module does not exist",
	[MSG_LOAD_FAIL]	    = "load failed",
	[MSG_MODULE_CAPPED] = "to many modules",

	// reserved for dynamic error
	[MSG_DYNAMIC]	    = NULL,
};

static struct jy_defs module_defs[MODULE_COUNT_LIMIT]	 = { NULL };
static void	     *module_handles[MODULE_COUNT_LIMIT] = { NULL };
static int	      module_count			 = 0;

#ifdef __unix__
#	include <dlfcn.h>

int jry_module_load(const char *path)
{
	loadfn_t load;

	int module = module_count;

	if (module >= MODULE_COUNT_LIMIT)
		return TO_ERROR(MSG_MODULE_CAPPED);

	int  pathsz   = strlen(path);
	char suffix[] = ".so";
	char modulepath[pathsz + sizeof(suffix)];

	strcpy(modulepath, path);
	strcat(modulepath, suffix);

	void *handle = dlopen(modulepath, RTLD_LAZY);

	if (handle == NULL)
		goto DLOAD_ERROR;

	load = (loadfn_t) (uintptr_t) dlsym(handle, "module_load");

	if (load == NULL)
		goto DLOAD_ERROR;

	int status = load(module);

	if (status != 0)
		return TO_ERROR(MSG_LOAD_FAIL);

	module_count += 1;
	return module;

DLOAD_ERROR:
	error_reason[MSG_DYNAMIC] = dlerror();
	return TO_ERROR(MSG_DYNAMIC);
}

struct jy_defs *jry_module_def(int module)
{
	assert(module >= 0 && module < MODULE_COUNT_LIMIT);
	return &module_defs[module];
}

void jry_module_unload(int module)
{
	assert(module >= 0 && module < MODULE_COUNT_LIMIT);

	int	       ret    = 0;
	void	      *handle = module_handles[module];
	struct jy_defs def    = module_defs[module];

	if (handle == NULL)
		goto FINISH;

	unloadfn_t unload;

	unload = (unloadfn_t) (uintptr_t) dlsym(handle, "module_unload");

	if (unload == NULL)
		goto CLOSE;

	unload(module);

CLOSE: {
	int status = dlclose(handle);
	ret	   = (status != 0) ? status : ret;
	jry_free_def(def);
}
FINISH:
	return;
}

const char *jry_module_error(int module)
{
	int	    err = TO_INDEX(module);
	const char *msg = error_reason[err];

	if (msg == NULL)
		return error_reason[MSG_UNKNOWN];

	return msg;
}

#endif // __unix__

// > user modules API
int define_function(int			module,
		    const char	       *key,
		    size_t		keysz,
		    struct jy_obj_func *func)
{
	if (module >= MODULE_COUNT_LIMIT || module < 0)
		return TO_ERROR(MSG_UNKNOWN);

	jy_val_t	v   = jry_func2v(func);
	struct jy_defs *def = &module_defs[module];

	if (jry_add_def(def, key, keysz, v, JY_K_FUNC) != 0)
		return TO_ERROR(MSG_UNKNOWN);

	return 0;
}

const char *error_message(int status)
{
	return jry_module_error(status);
}

// < user modules API
