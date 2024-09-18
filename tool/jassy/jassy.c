#include "compiler.h"
#include "dload.h"

#include "jary/memory.h"
#include "jary/object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define find_data(__list, __length, __val, __id)                               \
	for (uint32_t i = 0; i < (__length); ++i) {                            \
		if ((void *) (__val) == &(__list[i])) {                        \
			(__id) = i;                                            \
			break;                                                 \
		}                                                              \
	}

static const char *tkn2string(enum jy_tkn type)
{
	switch (type) {
	case TKN_NONE:
		return "TKN_NONE";
	case TKN_ERR:
		return "TKN_ERR";
	case TKN_ERR_STR:
		return "TKN_ERR_STR";
	case TKN_LEFT_PAREN:
		return "TKN_LEFT_PAREN";
	case TKN_RIGHT_PAREN:
		return "TKN_RIGHT_PAREN";
	case TKN_LEFT_BRACE:
		return "TKN_LEFT_BRACE";
	case TKN_RIGHT_BRACE:
		return "TKN_RIGHT_BRACE";
	case TKN_DOT:
		return "TKN_DOT";
	case TKN_COMMA:
		return "TKN_COMMA";
	case TKN_COLON:
		return "TKN_COLON";

	case TKN_SPACES:
		return "TKN_SPACES";
	case TKN_NEWLINE:
		return "TKN_NEWLINE";

	case TKN_IMPORT:
		return "TKN_IMPORT";
	case TKN_RULE:
		return "TKN_RULE";
	case TKN_INCLUDE:
		return "TKN_INCLUDE";
	case TKN_INGRESS:
		return "TKN_INGRESS";

	case TKN_JUMP:
		return "TKN_JUMP";
	case TKN_INPUT:
		return "TKN_INPUT";
	case TKN_MATCH:
		return "TKN_MATCH";
	case TKN_CONDITION:
		return "TKN_CONDITION";
	case TKN_FIELD:
		return "TKN_FIELD";

	case TKN_LONG_TYPE:
		return "TKN_LONG_TYPE";
	case TKN_STRING_TYPE:
		return "TKN_STRING_TYPE";

	case TKN_TILDE:
		return "TKN_TILDE";

	case TKN_PLUS:
		return "TKN_PLUS";
	case TKN_MINUS:
		return "TKN_MINUS";
	case TKN_STAR:
		return "TKN_STAR";
	case TKN_SLASH:
		return "TKN_SLASH";

	case TKN_EQUAL:
		return "TKN_EQUAL";
	case TKN_LESSTHAN:
		return "TKN_LESSTHAN";
	case TKN_GREATERTHAN:
		return "TKN_GREATERTHAN";
	case TKN_AND:
		return "TKN_AND";
	case TKN_OR:
		return "TKN_OR";
	case TKN_NOT:
		return "TKN_NOT";
	case TKN_ANY:
		return "TKN_ANY";
	case TKN_ALL:
		return "TKN_ALL";

	case TKN_REGEXP:
		return "TKN_REGEXP";
	case TKN_STRING:
		return "TKN_STRING";
	case TKN_NUMBER:
		return "TKN_NUMBER";
	case TKN_FALSE:
		return "TKN_FALSE";
	case TKN_TRUE:
		return "TKN_TRUE";

	case TKN_IDENTIFIER:
		return "TKN_IDENTIFIER";
	case TKN_DOLLAR:
		return "TKN_DOLLAR";
	case TKN_ALIAS:
		return "TKN_ALIAS";

	case TKN_CUSTOM:
		return "TKN_CUSTOM";
	case TKN_EOF:
		return "TKN_EOF";

	case TOTAL_TKN_TYPES:
		break;
	}

	return "UNKOWN";
}

static const char *ast2string(enum jy_ast type)
{
	switch (type) {
	case AST_NONE:
		return "NONE";
	case AST_ROOT:
		return "ROOT";
	case AST_RULE_DECL:
		return "RULE_DECL";
	case AST_IMPORT_STMT:
		return "IMPORT_STMT";
	case AST_INCLUDE_STMT:
		return "INCLUDE_STMT";
	case AST_INGRESS_DECL:
		return "INGRESS_DECL";
	case AST_NAME_DECL:
		return "NAME_DECL";
	case AST_LONG_TYPE:
		return "LONG_TYPE";
	case AST_STR_TYPE:
		return "STR_TYPE";
	case AST_FIELD_NAME:
		return "FIELD_NAME";
	case AST_JUMP_SECT:
		return "JUMP_SECT";
	case AST_INPUT_SECT:
		return "INPUT_SECT";
	case AST_MATCH_SECT:
		return "MATCH_SECT";
	case AST_CONDITION_SECT:
		return "CONDITION_SECT";
	case AST_FIELD_SECT:
		return "FIELD_SECT";
	case AST_EQUALITY:
		return "EQUALITY";
	case AST_LESSER:
		return "LESSER";
	case AST_GREATER:
		return "GREATER";
	case AST_ADDITION:
		return "ADDITION";
	case AST_SUBTRACT:
		return "SUBTRACT";
	case AST_MULTIPLY:
		return "MULTIPLY";
	case AST_DIVIDE:
		return "DIVIDE";
	case AST_REGMATCH:
		return "REGMATCH";
	case AST_NOT:
		return "NOT";
	case AST_AND:
		return "AND";
	case AST_OR:
		return "OR";
	case AST_REGEXP:
		return "REGEXP";
	case AST_STRING:
		return "STRING";
	case AST_LONG:
		return "LONG";
	case AST_FALSE:
		return "FALSE";
	case AST_TRUE:
		return "TRUE";
	case AST_ALIAS:
		return "ALIAS";
	case AST_FIELD:
		return "FIELD";
	case AST_CALL:
		return "CALL";
	case AST_EVENT:
		return "EVENT";
	case AST_NAME:
		return "NAME";
	case AST_PATH:
		return "PATH";
	case TOTAL_AST_TYPES:
		break;
	}

	return "UNKOWN";
}

static const char *k2string(enum jy_ktype type)
{
	switch (type) {
	case JY_K_TARGET:
		return "[TARGET]";
	case JY_K_MODULE:
		return "[MODULE]";
	case JY_K_LONG:
		return "[LONG]";
	case JY_K_STR:
		return "[STR]";
	case JY_K_FUNC:
		return "[FUNC]";
	case JY_K_EVENT:
		return "[EVENT]";
	case JY_K_BOOL:
		return "[BOOL]";
	case JY_K_INGRESS:
		return "[INGRESS]";
	case JY_K_RULE:
		return "[RULE]";
	case JY_K_UNKNOWN:
	default:
		return "[UNKNOWN]";
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
	case JY_OP_NOT:
		return "OP_NOT";
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

static inline uint32_t findmaxdepth(struct jy_asts *asts,
				    uint32_t	    id,
				    uint32_t	    depth)
{
	uint32_t *child	   = asts->child[id];
	uint32_t  childsz  = asts->childsz[id];
	uint32_t  maxdepth = depth;

	for (uint32_t i = 0; i < childsz; ++i) {
		uint32_t tmp = findmaxdepth(asts, child[i], depth + 1);
		maxdepth     = (tmp > maxdepth) ? tmp : maxdepth;
	}

	return maxdepth;
}

static void print_tkns(struct jy_tkns tkns)
{
	uint32_t maxtypesz = 0;

	for (uint32_t i = 0; i < tkns.size; ++i) {
		enum jy_tkn t	= tkns.types[i];
		const char *ts	= tkn2string(t);
		uint32_t    len = strlen(ts);
		maxtypesz	= len > maxtypesz ? len : maxtypesz;
	}

	printf("Lines  Offset   Type   %*cLexeme\n", maxtypesz - 4, ' ');

	for (uint32_t i = 0; i < tkns.size; ++i) {
		uint32_t    l	= tkns.lines[i];
		uint32_t    ofs = tkns.ofs[i];
		enum jy_tkn t	= tkns.types[i];
		const char *lex = tkns.lexemes[i];

		const char *ts	= tkn2string(t);

		printf("%5u   %5u   %s", l, ofs, ts);

		uint32_t n = strlen(ts);

		if (n != maxtypesz)
			printf("%*c", maxtypesz - n, ' ');

		printf("   ");

		switch (t) {
		case TKN_NONE:
		case TKN_NEWLINE:
		case TKN_SPACES:
			break;
		default:
			printf("%s", lex);
		}
		printf("\n");
	}
}

static inline void print_ast(struct jy_asts *asts,
			     char	   **lexemes,
			     uint32_t	     length,
			     uint32_t	     midpoint,
			     uint32_t	     numsz,
			     uint32_t	     id,
			     uint32_t	     depth)
{
	enum jy_ast type    = asts->types[id];
	uint32_t   *child   = asts->child[id];
	uint32_t    childsz = asts->childsz[id];
	uint32_t    tkn	    = asts->tkns[id];
	const char *typestr = ast2string(type);
	uint32_t    printed = 0;

	if (type != AST_ROOT) {
		printed	      += printf("|");
		uint32_t lnsz  = depth * 2 + 1;

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
	printed = printf("%d", id);
	printf("] ");
	diff = numsz - printed;

	if (diff > 0)
		printf("%*c", diff, ' ');

	printf("| ");

	if (tkn < length)
		printf("%s", lexemes[tkn]);

	printf("\n");

	for (uint32_t i = 0; i < childsz; ++i)
		print_ast(asts, lexemes, length, midpoint, numsz, child[i],
			  depth + 1);
}

static void print_asts(struct jy_asts *asts,
		       char	     **lexemes,
		       uint32_t	       length,
		       uint32_t	       maxdepth)
{
	uint32_t midpoint = 2 * maxdepth + 15;
	int	 col1sz	  = midpoint - 4;
	int	 idsz	  = snprintf(NULL, 0, "%d", asts->size);

	printf("Tree ");
	printf("%*c ", col1sz, ' ');
	printf(" ID ");
	printf(" %*c", idsz + 1, ' ');
	printf("Token\n");

	if (asts->size)
		print_ast(asts, lexemes, length, midpoint, idsz, 0, 0);
}

static inline int print_line(char    **lexemes,
			     uint32_t *lines,
			     uint32_t  size,
			     uint32_t  line)
{
	int printed = 0;
	for (uint32_t i = 0; i < size; ++i) {
		uint32_t l = lines[i];

		if (line < l)
			goto FINISH;

		if (l != line)
			continue;

		const char *lexeme = lexemes[i];

		if (*lexeme == '\n')
			continue;

		printed += printf("%s", lexeme);
	}

FINISH:
	return printed;
}

static inline void print_errors(struct jy_errs *errs,
				struct jy_tkns *tkns,
				const char     *path)
{
	for (uint32_t i = 0; i < errs->size; ++i) {
		uint32_t    from       = errs->from[i];
		uint32_t    to	       = errs->to[i];
		uint32_t    start_line = tkns->lines[to];
		uint32_t    ofs	       = tkns->ofs[to];
		const char *msg	       = errs->msgs[i];

		printf("%s:%d:%d error: %s\n", path, start_line, ofs, msg);

		uint32_t last_line = tkns->lines[from];
		printf("%5d | ", last_line);
		print_line(tkns->lexemes, tkns->lines, tkns->size, last_line);
		printf("\n");

		for (uint32_t j = from; j <= to; ++j) {
			uint32_t current_line = tkns->lines[j];

			if (current_line == last_line)
				goto NEXT_TKN;

			printf("%5d | ", current_line);
			print_line(tkns->lexemes, tkns->lines, tkns->size,
				   current_line);
			printf("\n");
NEXT_TKN:
			last_line = current_line;
		}

		printf("%5c | %*c\n", ' ', ofs, '^');
	}
}

static inline void print_defs(struct jy_defs *names,
			      struct jy_defs *events,
			      uint32_t	      eventsz,
			      int	     *modules,
			      uint32_t	      modulesz,
			      jy_funcptr_t   *call,
			      uint32_t	      callsz,
			      int	      indent)
{
	int maxtypesz = 0;
	int maxkeysz  = 0;

	for (uint32_t i = 0; i < names->capacity; ++i) {
		enum jy_ktype type = names->types[i];

		if (type == JY_K_UNKNOWN)
			continue;

		const char *ts	= k2string(type);
		int	    len = strlen(ts);
		int	    ks	= names->keysz[i];
		maxtypesz	= len > maxtypesz ? len : maxtypesz;
		maxkeysz	= ks > maxkeysz ? ks : maxkeysz;
	}

	for (uint32_t i = 0; i < names->capacity; ++i) {
		const char   *key  = names->keys[i];
		jy_val_t      v	   = names->vals[i];
		enum jy_ktype type = names->types[i];

		if (type == JY_K_UNKNOWN)
			continue;

		const char *typestr = k2string(type);
		uint32_t    id	    = 0;

		switch (type) {
		case JY_K_MODULE:
			find_data(modules, modulesz, v, id);
			break;
		case JY_K_EVENT:
			find_data(events, eventsz, v, id);
			break;
		case JY_K_FUNC:
			find_data(call, callsz, v, id);
			break;
		default:
			break;
		}

		if (indent)
			printf("%*c", indent, '\t');

		printf("%5u | ", i);
		int printed = printf("%s", typestr);

		if (printed < maxtypesz)
			printf("%*c", maxtypesz - printed, ' ');

		printf(" ");
		printed = printf("%s", key);

		if (printed < maxkeysz)
			printf("%*c", maxkeysz - printed, ' ');
		printf(" ");
		printf("%u\n", id);
	}
}

static inline void print_event(struct jy_defs *events)
{
	int maxtypesz = 0;
	int maxkeysz  = 0;

	for (uint32_t i = 0; i < events->capacity; ++i) {
		enum jy_ktype type = events->types[i];

		if (type == JY_K_UNKNOWN)
			continue;

		const char *ts	= k2string(type);
		int	    len = strlen(ts);
		int	    ks	= events->keysz[i];
		maxtypesz	= len > maxtypesz ? len : maxtypesz;
		maxkeysz	= ks > maxkeysz ? ks : maxkeysz;
	}

	for (uint32_t i = 0; i < events->capacity; ++i) {
		const char   *key  = events->keys[i];
		enum jy_ktype type = events->types[i];

		if (type == JY_K_UNKNOWN)
			continue;

		const char *typestr = k2string(type);

		printf("\t%5u | ", i);
		int printed = printf("%s", typestr);

		if (printed < maxtypesz)
			printf("%*c", maxtypesz - printed, ' ');

		printf(" ");
		printed = printf("%s", key);

		if (printed < maxkeysz)
			printf("%*c", maxkeysz - printed, ' ');

		printf("\n");
	}
}

static inline void print_events(struct jy_defs *events, uint16_t eventsz

)
{
	for (uint32_t i = 0; i < eventsz; ++i) {
		printf("%5u [EVENT] \n", i);
		print_event(&events[i]);
	}
}

static inline void print_modules(int		*modules,
				 uint32_t	 modulesz,
				 struct jy_defs *events,
				 uint32_t	 eventsz,
				 jy_funcptr_t	*call,
				 uint32_t	 callsz)
{
	for (uint32_t i = 0; i < modulesz; ++i) {
		printf("%5u [MODULE] \n", i);
		struct jy_defs *m = jry_module_def(modules[i]);
		print_defs(m, events, eventsz, NULL, 0, call, callsz, 1);
	}
}

static inline void print_calls(enum jy_ktype *types,
			       jy_val_t	     *vals,
			       uint16_t	      valsz)
{
	int		    maxtypesz = 0;
	int		    fnsz      = 0;
	struct jy_obj_func *fns[valsz];

	for (uint16_t i = 0; i < valsz; ++i) {
		if (types[i] != JY_K_FUNC)
			continue;

		struct jy_obj_func *ofunc  = jry_v2func(vals[i]);
		const char	   *ts	   = k2string(ofunc->return_type);
		int		    sz	   = strlen(ts);
		maxtypesz		   = sz > maxtypesz ? sz : maxtypesz;
		fns[fnsz]		   = ofunc;
		fnsz			  += 1;
	}

	for (uint16_t i = 0; i < fnsz; ++i) {
		struct jy_obj_func *ofunc   = fns[i];
		enum jy_ktype	    type    = ofunc->return_type;
		const char	   *typestr = k2string(type);

		printf("%5d | ", i);

		int printed = printf("%s", typestr);

		if (printed < maxtypesz)
			printf("%*c", maxtypesz - printed, ' ');

		for (uint8_t j = 0; j < ofunc->param_sz; ++j) {
			const char *ts = k2string(ofunc->param_types[j]);
			printf(" %s", ts);
		}

		printf("\n");
	}
}

static void print_kpool(enum jy_ktype *types, jy_val_t *vals, uint16_t valsz)
{
	int maxtypesz = 0;

	for (uint32_t i = 0; i < valsz; ++i) {
		const char *ts = k2string(types[i]);
		int	    sz = strlen(ts);
		maxtypesz      = sz > maxtypesz ? sz : maxtypesz;
	}

	uint16_t fncount = 0;

	for (uint32_t i = 0; i < valsz; ++i) {
		jy_val_t      val     = vals[i];
		enum jy_ktype type    = types[i];
		const char   *typestr = k2string(type);

		printf("%5d | ", i);

		int printed = printf("%s", typestr);

		if (printed < maxtypesz)
			printf("%*c", maxtypesz - printed, ' ');

		printf(" ");

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
			printf("%u", fncount);
			fncount += 1;
			break;
		}
		default:
			break;
		}

		printf("\n");
	}
}

static void print_chunks(uint8_t *codes, uint32_t codesz)
{
	for (uint32_t i = 0; i < codesz; ++i) {
		printf("%5d | ", i);

		enum jy_opcode code = codes[i];
		const char    *op   = codestring(code);

		printf("%s", op);

		switch (code) {
		case JY_OP_PUSH8:
			printf(" %d", codes[++i]);
			break;
		case JY_OP_JMPF:
		case JY_OP_JMPT: {
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

static uint32_t read_file(const char *path, char **dst)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	uint32_t file_size = ftell(file);
	rewind(file);

	char	*buffer	    = jry_alloc(file_size + 1);
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
	char	*src	     = NULL;
	uint32_t length	     = read_file(path, &src);

	struct jy_asts asts  = { .types = NULL };
	struct jy_tkns tkns  = { .types = NULL };
	struct jy_errs errs  = { .msgs = NULL };
	struct jy_defs names = { .keys = NULL };

	char dirname[]	     = "/modules/";
	char buf[strlen(dirpath) + sizeof(dirname)];

	strcpy(buf, dirpath);
	strcat(buf, dirname);

	struct jy_jay ctx = {
		.names = &names,
		.mdir  = buf,
	};

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
	       "\n\n");

	printf("Tokens"
	       "\n"
	       "===================================="
	       "\n\n");
	print_tkns(tkns);

	printf("\n\n");

	printf("Abstract Syntax Tree"
	       "\n"
	       "===================================="
	       "\n\n");
	uint32_t maxdepth = findmaxdepth(&asts, 0, 0);

	printf("File Path     : %s\n", path);
	printf("Maximum Depth : %d\n", maxdepth);
	printf("Total Nodes   : %d\n", asts.size);
	printf("\n");
	print_asts(&asts, tkns.lexemes, tkns.size, maxdepth);

	if (errs.size) {
		print_errors(&errs, &tkns, path);
		printf("\n");
		goto END;
	}

	printf("\n\n");

	printf("Jay VM Context"
	       "\n"
	       "===================================="
	       "\n\n");

	jry_compile(&asts, &tkns, &ctx, &errs);
	printf("Total Modules : %u\n", ctx.modulesz);
	printf("Constant Pool : %u\n", ctx.valsz);
	printf("Total Names   : %u\n", names.size);
	printf("Total Events  : %u\n", ctx.eventsz);
	printf("Total Chunk   : %u\n", ctx.codesz);

	printf("\n");

	printf("GLOBAL NAMES"
	       "\n"
	       "__________________\n\n");

	if (names.size) {
		print_defs(&names, ctx.events, ctx.eventsz, ctx.modules,
			   ctx.modulesz, ctx.call, ctx.callsz, 0);
		printf("\n");
	}

	printf("MODULE TABLE"
	       "\n"
	       "__________________\n\n");

	if (ctx.eventsz) {
		print_modules(ctx.modules, ctx.modulesz, ctx.events,
			      ctx.eventsz, ctx.call, ctx.callsz);
		printf("\n");
	}

	printf("EVENT TABLE"
	       "\n"
	       "__________________\n\n");

	if (ctx.eventsz) {
		print_events(ctx.events, ctx.eventsz);
		printf("\n");
	}

	printf("CALL TABLE"
	       "\n"
	       "__________________\n\n");

	if (ctx.callsz) {
		print_calls(ctx.types, ctx.vals, ctx.valsz);
		printf("\n");
	}

	printf("CONSTANT POOL"
	       "\n"
	       "__________________\n\n");

	if (ctx.valsz) {
		print_kpool(ctx.types, ctx.vals, ctx.valsz);
		printf("\n");
	}

	printf("BYTECODE"
	       "\n"
	       "__________________\n\n");

	if (ctx.codesz) {
		print_chunks(ctx.codes, ctx.codesz);
		printf("\n");
	}

	if (errs.size) {
		print_errors(&errs, &tkns, path);
		printf("\n");
	}

END:
	jry_free_asts(asts);
	jry_free_tkns(tkns);
	jry_free_errs(errs);
	jry_free_jay(ctx);
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
