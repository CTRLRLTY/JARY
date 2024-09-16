#include "jary/common.h"
#include "jary/modules.h"

static jy_val_t mark(int module, jy_val_t *vals, int count)
{
	return 0;
}

static enum jy_ktype	  markfn_param = JY_K_EVENT;
static struct jy_obj_func markfn       = { .return_type = JY_K_TARGET,
					   .internal	= true,
					   .param_types = &markfn_param,
					   .param_sz	= 1,
					   .func	= (jy_funcptr_t) mark };

int module_load(int module)
{
	define_function(module, "mark", 5, &markfn);

	return 0;
}

int module_unload(int __unused(module))
{
	return 0;
}
