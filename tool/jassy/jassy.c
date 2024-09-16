#include "compiler.h"

#include "jary/memory.h"
#include "jary/object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ast2string(enum jy_ast type)
{
	switch (type) {
	case AST_ROOT:
		return "root";
	case AST_RULE_DECL:
		return "rule";
	case AST_IMPORT_STMT:
		return "import";
	case AST_INCLUDE_STMT:
		return "include";
	case AST_INGRESS_DECL:
		return "ingress";
	case AST_NAME_DECL:
		return "declare";
	case AST_LONG_TYPE:
		return "long";
	case AST_STR_TYPE:
		return "string";
	case AST_FIELD_NAME:
		return "field";
	case AST_JUMP_SECT:
		return "target";
	case AST_INPUT_SECT:
		return "input";
	case AST_MATCH_SECT:
		return "match";
	case AST_CONDITION_SECT:
		return "condition";
	case AST_FIELD_SECT:
		return "fields";
	case AST_EQUALITY:
		return "equal";
	case AST_LESSER:
		return "less";
	case AST_GREATER:
		return "greater";
	case AST_ADDITION:
		return "add";
	case AST_SUBTRACT:
		return "sub";
	case AST_MULTIPLY:
		return "mul";
	case AST_DIVIDE:
		return "div";
	case AST_REGMATCH:
		return "regmatch";
	case AST_NOT:
		return "not";
	case AST_REGEXP:
		return "regex";
	case AST_STRING:
		return "string";
	case AST_LONG:
		return "long";
	case AST_FALSE:
		return "false";
	case AST_TRUE:
		return "true";
	case AST_ALIAS:
		return "alias";
	case AST_FIELD:
		return "field";
	case AST_CALL:
		return "call";
	case AST_EVENT:
		return "event";
	case AST_NAME:
		return "name";
	case AST_PATH:
		return "path";
	default:
		return "unknown";
	}
}

static const char *k2string(enum jy_ktype type)
{
	switch (type) {
	case JY_K_TARGET:
		return "[target]";
	case JY_K_MODULE:
		return "[module]";
	case JY_K_LONG:
		return "[long]";
	case JY_K_STR:
		return "[str]";
	case JY_K_FUNC:
		return "[func]";
	case JY_K_EVENT:
		return "[event]";
	case JY_K_BOOL:
		return "[bool]";
	case JY_K_INGRESS:
		return "[ingress]";
	case JY_K_RULE:
		return "[rule]";
	case JY_K_UNKNOWN:
		return "[unknown]";
	default:
		return "[unknown]";
	}
}

static const char *codestring(enum jy_opcode code)
{
	switch (code) {
	case JY_OP_PUSH8:
		return "OP_PUSH8";
	case JY_OP_PUSH16:
		return "OP_PUSH16";
	case JY_OP_PUSH32:
		return "OP_PUSH32";
	case JY_OP_PUSH64:
		return "OP_PUSH64";
	case JY_OP_ADD:
		return "OP_ADD";
	case JY_OP_SUB:
		return "OP_SUB";
	case JY_OP_MUL:
		return "OP_MUL";
	case JY_OP_DIV:
		return "OP_DIV";
	case JY_OP_GT:
		return "OP_GT";
	case JY_OP_LT:
		return "OP_LT";
	case JY_OP_CMP:
		return "OP_CMP";
	case JY_OP_JMPF:
		return "OP_JMPF";
	case JY_OP_JMPT:
		return "OP_JMPT";
	case JY_OP_CALL:
		return "OP_CALL";
	case JY_OP_END:
		return "OP_END";
	default:
		return "OP_UNKNOWN";
	}
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

static void print_ast(struct jy_asts *asts,
		      char	    **lexemes,
		      size_t	      length,
		      size_t	      midpoint,
		      size_t	      numsz,
		      size_t	      id,
		      size_t	      depth)
{
	enum jy_ast type    = asts->types[id];
	size_t	   *child   = asts->child[id];
	size_t	    childsz = asts->childsz[id];
	size_t	    tkn	    = asts->tkns[id];
	const char *typestr = ast2string(type);
	size_t	    printed = 0;

	if (type != AST_ROOT) {
		printed	    += printf("|");
		size_t lnsz  = depth * 2 + 1;

		char depthline[lnsz];
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

	for (size_t i = 0; i < childsz; ++i)
		print_ast(asts, lexemes, length, midpoint, numsz, child[i],
			  depth + 1);
}

static void print_asts(struct jy_asts *asts,
		       char	     **lexemes,
		       size_t	       length,
		       size_t	       maxdepth)
{
	size_t midpoint = 2 * maxdepth + 15;
	int    col1sz	= midpoint - 4;
	int    idsz	= snprintf(NULL, 0, "%ld", asts->size);

	printf("Tree ");
	printf("%*c ", col1sz, ' ');
	printf(" ID ");
	printf(" %*c", idsz + 1, ' ');
	printf("Token\n");

	if (asts->size)
		print_ast(asts, lexemes, length, midpoint, idsz, 0, 0);
}

static inline void print_tkn_line(struct jy_tkns *tkns, uint32_t line)
{
	for (uint32_t i = 0; i < tkns->size; ++i) {
		uint32_t l = tkns->lines[i];

		if (l != line)
			continue;

		enum jy_tkn type = tkns->types[i];

		if (type == TKN_NEWLINE)
			continue;

		const char *lexeme = tkns->lexemes[i];
		printf("%s", lexeme);
	}
}

static inline void print_tkn_errs(struct jy_errs *errs,
				  struct jy_tkns *tkns,
				  const char	 *path)
{
	for (size_t i = 0; i < errs->size; ++i) {
		size_t	    tkn	   = errs->ids[i];
		size_t	    line   = tkns->lines[tkn];
		uint32_t    ofs	   = tkns->ofs[tkn];
		uint32_t    lexsz  = tkns->lexsz[tkn];
		const char *lexeme = tkns->lexemes[tkn];
		const char *msg	   = errs->msgs[i];

		printf("%s:%ld:%d error: %s ", path, line, ofs, msg);
		printf("'%s'\n", lexeme);

		printf("%5ld | ", line);

		print_tkn_line(tkns, line);
		printf("\n");

		printf("%5c | %*c", ' ', ofs - lexsz + 1, '^');
	}
}

static inline void print_names(struct jy_defs *names, int indent)
{
	for (uint32_t i = 0; i < names->capacity; ++i) {
		const char   *key  = names->keys[i];
		jy_val_t      v	   = names->vals[i];
		enum jy_ktype type = names->types[i];

		if (type == JY_K_UNKNOWN)
			continue;

		const char *typestr = k2string(type);

		if (indent)
			printf("%*c", indent, ' ');

		printf("%5u | %s %s\n", i, typestr, key);

		switch (type) {
		case JY_K_MODULE:
		case JY_K_EVENT:
			print_names(jry_v2def(v), indent + 7);
			break;
		default:
			break;
		}
	}
}

static inline void print_ast_err(struct jy_asts *asts,
				 struct jy_tkns *tkns,
				 size_t		 prev_line,
				 size_t		 ast)
{
	size_t	*child	 = asts->child[ast];
	size_t	 childsz = asts->childsz[ast];
	size_t	 tkn	 = asts->tkns[ast];
	uint32_t line	 = tkns->lines[tkn];

	if (line > prev_line) {
		printf("\n%5u | ", line);
		print_tkn_line(tkns, line);
	}

	for (size_t i = 0; i < childsz; ++i) {
		uint32_t cast = child[i];
		print_ast_err(asts, tkns, line, cast);
	}
}

static inline void find_last_tkn(struct jy_asts *asts,
				 size_t		*lines,
				 uint32_t	 prev_line,
				 size_t		 ast,
				 size_t		*lasttkn)
{
	size_t	*child	 = asts->child[ast];
	uint32_t childsz = asts->childsz[ast];
	size_t	 tkn	 = asts->tkns[ast];
	uint32_t line	 = lines[tkn];

	if (line > prev_line)
		*lasttkn = tkn;

	for (uint32_t i = 0; i < childsz; ++i)
		find_last_tkn(asts, lines, line, child[i], lasttkn);
}

static inline void print_ast_errs(struct jy_errs *errs,
				  struct jy_asts *asts,
				  struct jy_tkns *tkns,
				  const char	 *path)
{
	for (size_t i = 0; i < errs->size; ++i) {
		size_t	    ast = errs->ids[i];
		const char *msg = errs->msgs[i];

		size_t tkn;
		find_last_tkn(asts, tkns->lines, 0, ast, &tkn);

		uint32_t    line   = tkns->lines[tkn];
		uint32_t    ofs	   = tkns->ofs[tkn];
		const char *lexeme = tkns->lexemes[tkn];
		uint32_t    lexsz  = tkns->lexsz[tkn];

		printf("%s:%d:%d error: %s ", path, line, ofs, msg);
		printf("'%s'", lexeme);

		print_ast_err(asts, tkns, 0, ast);
		printf("\n");
		printf("%5c | %*c", ' ', ofs - lexsz + 1, '^');
		printf("\n");
	}
}

static void print_kpool(enum jy_ktype *types, jy_val_t *vals, uint32_t valsz)
{
	for (size_t i = 0; i < valsz; ++i) {
		jy_val_t      val     = vals[i];
		enum jy_ktype type    = types[i];
		const char   *typestr = k2string(type);

		printf("%5ld | ", i);

		printf("%s ", typestr);

		switch (type) {
		case JY_K_LONG:
			printf("%ld", jry_v2long(val));
			break;
		case JY_K_STR:
			printf("%s", jry_v2str(val)->str);
			break;
		case JY_K_EVENT: {
			struct jy_obj_event ev = jry_v2event(val);
			printf("%u %u", ev.event, ev.name);
			break;
		}
		case JY_K_FUNC: {
			const char *s = k2string(jry_v2func(val)->return_type);
			printf("%s", s);
			break;
		}
		default:
			break;
		}

		printf("\n");
	}
}

static void print_chunks(uint8_t *codes, size_t codesz)
{
	for (size_t i = 0; i < codesz; ++i) {
		printf("%5ld | ", i);

		enum jy_opcode code = codes[i];
		const char    *op   = codestring(code);

		printf("%s", op);

		switch (code) {
		case JY_OP_PUSH8:
			printf(" %d", codes[++i]);
			break;
		case JY_OP_JMPF: {
			union {
				short	*num;
				uint8_t *code;
			} ofs;

			ofs.code  = &codes[i + 1];
			i	 += 2;
			printf(" %d", *ofs.num);
			break;
		}
		case JY_OP_CALL: {
			union {
				short	*num;
				uint8_t *code;
			} ofs;

			uint8_t paramsz	 = codes[++i];
			ofs.code	 = &codes[i + 1];
			i		+= 2;

			printf(" %u %u", paramsz, *ofs.num);
			break;
		}
		default:
			break;
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

static void run_file(const char *path, const char *dirpath)
{
	char  *src	    = NULL;
	size_t length	    = read_file(path, &src);

	struct jy_asts asts = { .types = NULL };
	struct jy_tkns tkns = { .types = NULL };
	struct jy_errs errs = { .msgs = NULL };

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
	print_asts(&asts, tkns.lexemes, tkns.size, maxdepth);

	printf("\n");
	printf("File Path     : %s\n", path);
	printf("Maximum Depth : %ld\n", maxdepth);
	printf("Total Nodes   : %ld\n", asts.size);
	printf("Token Errors  : %u\n", errs.size);

	print_tkn_errs(&errs, &tkns, path);

	struct jy_defs names = { .keys = NULL };

	char dirname[]	     = "/modules/";
	char buf[strlen(dirpath) + sizeof(dirname)];

	strcpy(buf, dirpath);
	strcat(buf, dirname);

	struct jy_scan_ctx ctx = {
		.names = &names,
		.mdir  = buf,
	};

	if (errs.size)
		goto END;

	jry_compile(&asts, &tkns, &ctx, &errs);

	printf("Total Modules : %u\n", ctx.modulesz);
	printf("Constant Pool : %u\n", ctx.valsz);
	printf("Total Names   : %u\n", names.size);
	printf("Total Events  : %u\n", ctx.eventsz);
	printf("Total Chunk   : %u\n", ctx.codesz);
	printf("AST Errors    : %u\n", errs.size);

	if (errs.size) {
		print_ast_errs(&errs, &asts, &tkns, path);
		goto END;
	}

	printf("\n");

	printf("___GLOBAL NAMES____\n\n");
	print_names(&names, 0);
	printf("\n");

	printf("___CONSTANT POOL___\n\n");
	if (ctx.valsz) {
		print_kpool(ctx.types, ctx.vals, ctx.valsz);
		printf("\n");
	}

	printf("___BYTE CODE_______\n\n");

	if (ctx.codesz) {
		print_chunks(ctx.codes, ctx.codesz);
		printf("\n");
	}

END:

	printf("\n");
	jry_free_asts(asts);
	jry_free_tkns(tkns);
	jry_free_errs(errs);
	jry_free_scan_ctx(ctx);
}

int main(int argc, const char **argv)
{
	const char *binpath = argv[0];
	const char *dirp    = strrchr(argv[0], '/');

	int  dirsz	    = (dirp != NULL) ? dirp - binpath + 1 : 1;
	char dirpath[dirsz];

	memcpy(dirpath, binpath, dirsz);

	dirpath[dirsz - 1] = '\0';

	if (argc == 2)
		run_file(argv[1], dirpath);
	else
		fprintf(stderr, "require file path");

	return 0;
}
