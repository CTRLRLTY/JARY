#include "compiler.h"

#include "jary/error.h"
#include "jary/memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ast2string(enum jy_ast type)
{
	char *buf = NULL;

	switch (type) {
	case AST_ROOT:
		buf = strdup("root");
		break;
	case AST_RULE:
		buf = strdup("rule");
		break;
	case AST_IMPORT:
		buf = strdup("import");
		break;
	case AST_INCLUDE:
		buf = strdup("include");
		break;
	case AST_INGRESS:
		buf = strdup("include");
		break;

	case AST_JUMP:
		buf = strdup("target");
		break;
	case AST_INPUT:
		buf = strdup("input");
		break;
	case AST_MATCH:
		buf = strdup("match");
		break;
	case AST_CONDITION:
		buf = strdup("condition");
		break;
	case AST_FIELDS:
		buf = strdup("fields");
		break;

	case AST_MEMBER:
		buf = strdup("member");
		break;
	case AST_ALIAS:
		buf = strdup("alias");
		break;

	case AST_EQUALITY:
		buf = strdup("equal");
		break;
	case AST_LESSER:
		buf = strdup("lesss");
		break;
	case AST_GREATER:
		buf = strdup("greater");
		break;

	case AST_ADDITION:
		buf = strdup("add");
		break;
	case AST_SUBTRACT:
		buf = strdup("sub");
		break;
	case AST_MULTIPLY:
		buf = strdup("mul");
		break;
	case AST_DIVIDE:
		buf = strdup("div");
		break;

	case AST_REGMATCH:
		buf = strdup("regmatch");
		break;
	case AST_CALL:
		buf = strdup("call");
		break;

	case AST_NOT:
		buf = strdup("not");
		break;
	case AST_EVENT:
		buf = strdup("event");
		break;
	case AST_NAME:
		buf = strdup("name");
		break;
	case AST_PATH:
		buf = strdup("path");
		break;

	case AST_REGEXP:
		buf = strdup("regex");
		break;
	case AST_STRING:
		buf = strdup("string");
		break;
	case AST_LONG:
		buf = strdup("long");
		break;
	case AST_FALSE:
		buf = strdup("false");
		break;
	case AST_TRUE:
		buf = strdup("true");
		break;

	default:
		buf = strdup("unknown");
	}

	return buf;
}

static void printast(struct jy_asts *asts, char **lexemes, size_t length,
		     size_t midpoint, size_t numsz, size_t id, size_t depth)
{
	enum jy_ast type    = asts->types[id];
	size_t	   *child   = asts->child[id];
	size_t	    childsz = asts->childsz[id];
	size_t	    tkn	    = asts->tkns[id];

	char  *typestr	    = ast2string(type);
	size_t typestrsz    = strlen(typestr);
	size_t printed	    = 0;

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

	printf(" [");
	printed = printf("%ld", id);
	printf("] ");
	diff = numsz - printed;

	if (diff > 0)
		printf("%*c", diff, ' ');

	printf("| ");

	if (tkn < length)
		printf("%s", lexemes[tkn]);

	printf("\n");

	jry_free(typestr);

	for (size_t i = 0; i < childsz; ++i)
		printast(asts, lexemes, length, midpoint, numsz, child[i],
			 depth + 1);
}

static size_t findmaxdepth(struct jy_asts *asts, size_t id, size_t depth)
{
	size_t *child	 = asts->child[id];
	size_t	childsz	 = asts->childsz[id];
	size_t	maxdepth = depth;

	for (size_t i = 0; i < childsz; ++i) {
		size_t tmp = findmaxdepth(asts, child[i], depth + 1);
		maxdepth   = (tmp > maxdepth) ? tmp : maxdepth;
	}

	return maxdepth;
}

static void dumpast(struct jy_asts *asts, char **lexemes, size_t length,
		    size_t maxdepth)
{
	size_t midpoint = 2 * maxdepth + 15;
	int    col1sz	= midpoint - 4;

	int idsz	= snprintf(NULL, 0, "%ld", asts->size);

	printf("Tree ");
	printf("%*c ", col1sz, ' ');
	printf(" ID ");
	printf(" %*c", idsz + 1, ' ');
	printf("Token\n");

	if (asts->size)
		printast(asts, lexemes, length, midpoint, idsz, 0, 0);
}

inline static void dumperrs(struct jy_prserrs *errs, const char *path)
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

static void dumpkpool(struct jy_kpool *kpool)
{
	for (size_t i = 0; i < kpool->size; ++i) {
		jy_val_t      val  = kpool->vals[i];
		enum jy_ktype type = kpool->types[i];

		printf("[%ld] ", i);

		switch (type) {
		case JY_K_LONG:
			printf("LONG %ld", jry_val_long(val));
			break;

		default:
			break;
		}

		printf("\n");
	}
}

static void dumpcnks(struct jy_chunks *cnk)
{
	for (size_t i = 0; i < cnk->size; ++i) {
		enum jy_opcode code = cnk->codes[i];

		switch (code) {
		case JY_OP_PUSH8:
			printf("OP_PUSH8 %d", cnk->codes[++i]);
			break;
		case JY_OP_CMP:
			printf("OP_CMP");
			break;
		default:
			printf("OP_UNKNOWN");
		}

		printf("\n");
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
	char		 *src	 = NULL;
	size_t		  length = read_file(path, &src);
	size_t		  depth	 = 0;
	struct jy_asts	  asts	 = { .size = 0 };
	struct jy_tkns	  tkns	 = { .size = 0 };
	struct jy_prserrs errs	 = { .size = 0 };

	jry_parse(src, length, &asts, &tkns, &errs);
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

	size_t maxdepth = findmaxdepth(&asts, 0, 0);
	dumpast(&asts, tkns.lexemes, tkns.size, maxdepth);

	printf("\n");
	printf("File Path     : %s\n", path);
	printf("Maximum Depth : %ld\n", maxdepth);
	printf("Total nodes   : %ld\n", asts.size);

	if (errs.size) {
		printf("ERRORS FOUND  : %ld\n", errs.size);

		dumperrs(&errs, path);
	}

	printf("\n");

	struct jy_kpool	 kpool = { NULL };
	struct jy_names	 names = { NULL };
	struct jy_chunks cnk   = { NULL };

	struct jy_scan_ctx ctx = { .pool  = &kpool,
				   .names = &names,
				   .cnk	  = &cnk };

	jry_compile(&asts, &tkns, &ctx, NULL);

	printf("___CONSTANT POOL___\n\n");
	if (ctx.pool->size) {
		dumpkpool(ctx.pool);
		printf("\n");
	}

	printf("___BYTE CODE_______\n\n");

	if (ctx.cnk->size) {
		dumpcnks(ctx.cnk);
		printf("\n");
	}

	jry_free_asts(&asts);
	jry_free_tkns(&tkns);
	jry_free_prserrs(&errs);
	jry_free_scan_ctx(&ctx);
}

int main(int argc, const char **argv)
{
	if (argc == 2)
		run_file(argv[1]);
	else
		fprintf(stderr, "require file path");

	return 0;
}