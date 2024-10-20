#include "jary/jary.h"
#include "jary/memory.h"

#include <sys/socket.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint32_t read_file(struct sc_mem *alloc,
				 const char    *path,
				 char	      **dst)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	uint32_t file_size = ftell(file);
	rewind(file);

	char	*buffer	    = sc_alloc(alloc, file_size + 1);
	uint32_t bytes_read = fread(buffer, sizeof(char), file_size, file);

	if (bytes_read < file_size) {
		fprintf(stderr, "could not read file \"%s\".\n", path);
		exit(74);
	}

	buffer[bytes_read] = '\0';
	*dst		   = buffer;

	fclose(file);
	return file_size + 1;
}

static void run_file(const char *path, const char *dirpath)
{
	struct sc_mem alloc  = { .buf = NULL };
	char	     *src    = NULL;
	uint32_t      length = read_file(&alloc, path, &src);
	struct jary  *jary;

	jary_open(&jary);

	jary_compile(jary, length, src, dirpath);

	jary_close(jary);

	sc_free(&alloc);
}

int main(int argc, const char **argv)
{
	if (argc == 3)
		run_file(argv[1], argv[2]);
	else
		fprintf(stderr, "jaryd [file] [modulepath]");

	return 0;
}
