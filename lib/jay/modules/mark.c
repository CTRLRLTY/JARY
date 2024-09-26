#include "jary/modules.h"

static jy_val_t mark(jy_val_t *vals, int count)
{
	return 0;
}

int module_load(struct jy_module *ctx)
{
	enum jy_ktype params = JY_K_STR;

	define_function(ctx, "mark", JY_K_TARGET, 1, &params,
			(jy_funcptr_t) mark);

	return 0;
}

int module_unload(void)
{
	return 0;
}
