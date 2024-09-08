#include "dload.h"

#include "jary/error.h"
#include "jary/memory.h"

#include <stdint.h>
#include <string.h>

typedef int (*loadfn_t)(struct jy_module_ctx *);
typedef int (*unloadfn_t)(struct jy_module_ctx *);

#ifdef __unix__
#	include <dlfcn.h>

struct jy_module_ctx {
	void	       *handle;
	struct jy_defs *defs;
};

int jry_module_load(const char		  *path,
		    struct jy_defs	 **defs,
		    struct jy_module_ctx **ctx)
{
	loadfn_t load;

	int  pathsz   = strlen(path);
	char suffix[] = ".so";
	char modulepath[pathsz + sizeof(suffix)];

	strcpy(modulepath, path);
	strcat(modulepath, suffix);

	void	   *handle = dlopen(modulepath, RTLD_LAZY);
	const char *errmsg = dlerror();

	if (handle == NULL)
		return ERROR_OPEN_MODULE;

	load = (loadfn_t) (uintptr_t) dlsym(handle, "module_load");

	if (load == NULL)
		return ERROR_LOAD_MODULE;

	struct jy_defs	     d	    = { NULL };
	struct jy_module_ctx c	    = { .handle = handle, .defs = &d };
	int		     status = load(&c);

	if (status != ERROR_SUCCESS)
		return status;

	*ctx   = jry_alloc(sizeof *ctx);
	*defs  = jry_alloc(sizeof d);
	**defs = d;
	c.defs = *defs;
	**ctx  = c;
	return ERROR_SUCCESS;
}

int jry_module_unload(struct jy_module_ctx *ctx)
{
	int ret = ERROR_SUCCESS;

	if (ctx == NULL)
		goto FINISH;

	jry_assert(ctx->handle != NULL);
	jry_assert(ctx->defs != NULL);

	unloadfn_t unload;

	unload = (unloadfn_t) (uintptr_t) dlsym(ctx->handle, "module_unload");

	if (unload == NULL) {
		ret = ERROR_LOAD_MODULE;
		goto CLOSE;
	}

	ret = unload(ctx);
CLOSE: {
	int status = dlclose(ctx->handle);
	ret	   = (status != 0) ? status : ret;
	jry_free_def(ctx->defs);
	jry_free(ctx->defs);
	jry_free(ctx);
}
FINISH:
	return ret;
}

int define_function(struct jy_module_ctx *ctx,
		    const char		 *key,
		    size_t		  keysz,
		    struct jy_obj_func	 *func)
{
	int status =
		jry_add_def(ctx->defs, key, keysz, jry_func2v(func), JY_K_FUNC);

	return status;
}
#endif // __unix__