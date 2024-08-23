
#include "error.h"
#include "memory.h"
#include "parser.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ast2string(jy_ast_type_t type)
{
	char *buf = NULL;

	switch (type) {
	case AST_ROOT:
		buf = strdup("root");
		break;
	case AST_DECL:
		buf = strdup("decl");
		break;
	case AST_SECTION:
		buf = strdup("section");
		break;
	case AST_BINARY:
		buf = strdup("binary");
		break;
	case AST_CALL:
		buf = strdup("call");
		break;
	case AST_UNARY:
		buf = strdup("unary");
		break;
	case AST_EVENT:
		buf = strdup("event");
		break;
	case AST_MEMBER:
		buf = strdup("member");
		break;
	case AST_NAME:
		buf = strdup("name");
		break;
	case AST_PATH:
		buf = strdup("path");
		break;
	case AST_LITERAL:
		buf = strdup("literal");
		break;
	default:
		buf = strdup("unknown");
	}

	return buf;
}

static void printast(jy_asts_t *asts, char **lexemes, size_t length,
		     size_t maxdepth, size_t id, size_t depth)
{
	size_t midpoint = 2 * maxdepth + 15;
	int    idofs	= (asts->size) ? (int) log10((double) asts->size) : 0;

	jy_ast_type_t type    = asts->types[id];
	size_t	     *child   = asts->child[id];
	size_t	      childsz = asts->childsz[id];
	size_t	      tkn     = asts->tkns[id];

	char  *typestr	      = ast2string(type);
	size_t typestrsz      = strlen(typestr);
	size_t printed	      = 0;

	if (type != AST_ROOT) {
		printed	    += printf("|");
		size_t lnsz  = depth * 2 + 1;
		char   depthline[lnsz];
		memset(depthline, '_', lnsz);
		depthline[lnsz - 1]  = '\0';
		printed		    += printf("%s ", depthline);
	}

	printed	 += printf("%s ", typestr);

	int diff  = midpoint - printed;

	if (diff > 0) {
		char dots[diff + 1];
		memset(dots, '.', diff);
		dots[diff]  = '\0';
		printed	   += printf("%s |", dots);
	}

	printed	      += printf(" [%ld] ", id);
	size_t vidofs  = id / 10;
	diff	       = idofs - vidofs;

	if (diff > 0)
		printed += printf("%*c", diff, ' ');

	printed += printf("| ");

	if (tkn < length)
		printf("%s", lexemes[tkn]);

	printf("\n");

	jry_free(typestr);

	for (size_t i = 0; i < childsz; ++i)
		printast(asts, lexemes, length, maxdepth, child[i], depth + 1);
}

static void dumpast(jy_asts_t *asts, char **lexemes, size_t length,
		    size_t maxdepth)
{
	size_t midpoint = 2 * maxdepth + 15;
	int    idofs	= asts->size / 10;
	int    col1sz	= midpoint - 4;

	printf("Tree ");
	printf("%*c ", col1sz, ' ');
	printf(" ID ");
	printf("%*c", 3 + idofs, ' ');
	printf("Token\n");

	if (asts->size)
		printast(asts, lexemes, length, maxdepth, 0, 0);
}

inline static void dumperrs(jy_parse_errs_t *errs, const char *path)
{
	for (size_t i = 0; i < errs->size; ++i) {
		size_t line   = errs->lines[i];
		size_t ofs    = errs->ofs[i];
		char  *lexeme = errs->lexemes[i];
		char  *msg    = errs->msgs[i];

		printf("%s:%ld:%ld %s\n", path, line, ofs, msg);

		if (lexeme) {
			printf("%5ld |\t", line);
			printf("%s\n", lexeme);
		}
	}
}

static size_t read_file(const char *path, char **dst)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	size_t file_size = ftell(file);
	rewind(file);

	char  *buffer	  = jry_alloc(file_size + 1);
	size_t bytes_read = fread(buffer, sizeof(char), file_size, file);

	if (bytes_read < file_size) {
		fprintf(stderr, "could not read file \"%s\".\n", path);
		exit(74);
	}

	buffer[bytes_read] = '\0';
	*dst		   = buffer;

	fclose(file);
	return file_size + 1;
}

static void run_file(const char *path)
{
	char	       *src    = NULL;
	size_t		length = read_file(path, &src);
	size_t		depth  = 0;
	jy_asts_t	asts   = { .size = 0 };
	jy_tkns_t	tkns   = { .size = 0 };
	jy_parse_errs_t errs   = { .size = 0 };

	jry_parse(src, length, &asts, &tkns, &errs, &depth);
	jry_free(src);

	printf("===================================="
	       "\n"
	       "|                                  |"
	       "\n"
	       "| JARY ABSTRACT SYNTAX TREE DUMP ! |"
	       "\n"
	       "|                                  |"
	       "\n"
	       "===================================="
	       "\n");

	printf("\n");

	dumpast(&asts, tkns.lexemes, tkns.size, depth);

	printf("\n");
	printf("File Path     : %s\n", path);
	printf("Maximum Depth : %ld\n", depth);
	printf("Total nodes   : %ld\n", asts.size);
	printf("ERRORS FOUND  : %ld\n", errs.size);

	dumperrs(&errs, path);
	printf("\n");

	jry_free_asts(&asts);
	jry_free_tkns(&tkns);
	jry_free_parse_errs(&errs);
}

int main(int argc, const char **argv)
{
	if (argc == 2)
		run_file(argv[1]);
	else
		fprintf(stderr, "require file path");

	return 0;
}