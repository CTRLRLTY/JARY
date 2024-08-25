#ifndef FLAP_H
#define FLAP_H

#include <stddef.h>

enum flap_arg {
	FLAP_ARG_INT,
	FLAP_ARG_STRING,
	FLAP_ARG_BOOL,
};

void flap_args(const char **args, size_t length);

void flap_opt(char opt, const char *alias, size_t length, const char *desc,
	      size_t dscsz);

#endif // FLAP_H