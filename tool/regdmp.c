#include "jay/regex.c"

#include "jary/memory.h"

#include <stdio.h>

static inline char *tknstr(enum TKN type)
{
	switch (type) {
	case TKN_BACKSLASH:
		return "BACKSLASH";
	case TKN_CARET:
		return "CARET";
	case TKN_SINGLE:
		return "SINGLE";
	case TKN_LEFT_BRACKET:
		return "LEFT_BRACKET";
	case TKN_RIGHT_BRACKET:
		return "RIGHT_BRACKET";
	case TKN_LEFT_PAREN:
		return "LEFT_PAREN";
	case TKN_RIGHT_PAREN:
		return "RIGHT_PAREN";
	case TKN_LEFT_BRACE:
		return "LEFT_BRACE";
	case TKN_RIGHT_BRACE:
		return "RIGHT_BRACE";
	case TKN_VERTBAR:
		return "VERTBAR";
	case TKN_QMARK:
		return "QMARK";
	case TKN_DOT:
		return "DOT";
	case TKN_PLUS:
		return "PLUS";
	case TKN_STAR:
		return "STAR";
	case TKN_DOLLAR:
		return "DOLLAR";
	case TKN_COMMA:
		return "COMMA";
	case TKN_ESCAPED:
		return "ESCAPED";
	case TKN_EOF:
		return "EOF";
	}
}

static void pr_tknlist(uint32_t		     size,
		       uint32_t		    *offsets,
		       uint16_t		    *lexemesz,
		       const char *const    *lexemes,
		       const enum TKN *const types)
{
	uint32_t maxtypesz = 0;

	for (uint32_t i = 0; i < size; ++i) {
		enum TKN    t	= types[i];
		const char *ts	= tknstr(t);
		uint32_t    len = strlen(ts);
		maxtypesz	= len > maxtypesz ? len : maxtypesz;
	}

	printf("Offset   Type   %*cLexeme\n", maxtypesz - 4, ' ');

	for (uint32_t i = 0; i < size; ++i) {
		enum TKN    type   = types[i];
		const char *lex	   = lexemes[i];
		int	    lexsz  = lexemesz[i];
		const char *typstr = tknstr(type);

		printf("%5u    %s", offsets[i], typstr);

		uint32_t n = strlen(typstr);

		if (n != maxtypesz)
			printf("%*c", maxtypesz - n, ' ');

		printf("   ");
		printf("%.*s", lexsz, lex);
		printf("\n");
	}
}

static inline size_t findmaxdepth(rgxast_t	       id,
				  const struct rgxast *asts,
				  size_t	       depth)
{
	rgxast_t *child	   = asts->child[id];
	rgxast_t  childsz  = asts->childsz[id];
	size_t	  maxdepth = depth;

	for (uint32_t i = 0; i < childsz; ++i) {
		size_t tmp = findmaxdepth(child[i], asts, depth + 1);
		maxdepth   = (tmp > maxdepth) ? tmp : maxdepth;
	}

	return maxdepth;
}

static inline void pr_ast(rgxast_t	       id,
			  const struct rgxast *asts,
			  uint32_t	       midpoint,
			  uint32_t	       numsz,
			  uint32_t	       depth)
{
	enum RGXAST type    = asts->type[id];
	rgxast_t   *child   = asts->child[id];
	rgxast_t    childsz = asts->childsz[id];
	uint32_t    printed = 0;

	const char *typestr = "UNKNOWN";

	switch (type) {
	case RGXAST_ESCAPE:
		typestr = "ESCAPE";
		break;
	case RGXAST_SINGLE:
		typestr = "SINGLE";
		break;
	case RGXAST_OR:
		typestr = "OR";
		break;
	case RGXAST_CHARSET:
		typestr = "CHARSET";
		break;
	case RGXAST_ROOT:
		typestr = "ROOT";
		break;
	case RGXAST_PLUS:
		typestr = "PLUS";
		break;
	case RGXAST_QMARK:
		typestr = "QMARK";
		break;
	case RGXAST_STAR:
		typestr = "STAR";
		break;
	case RGXAST_CONCAT:
		typestr = "CONCAT";
		break;
	}

	if (type != RGXAST_ROOT) {
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

	printf("%c", asts->c[id]);

	printf("\n");

	for (uint32_t i = 0; i < childsz; ++i)
		pr_ast(child[i], asts, midpoint, numsz, depth + 1);
}

static inline void pr_astlist(const struct rgxast *asts)
{
	size_t depth	= findmaxdepth(0, asts, 0);
	size_t midpoint = 2 * depth + 20;
	int    col1sz	= midpoint - 4;
	int    idsz	= snprintf(NULL, 0, "%d", asts->size);

	printf("Tree ");
	printf("%*c ", col1sz, ' ');
	printf(" ID ");
	printf(" %*c", idsz + 1, ' ');
	printf("Token\n");

	if (asts->size)
		pr_ast(0, asts, midpoint, idsz, 0);
}

static void prcodes(long size, uint8_t *codes)
{
#define OFS() pc.u8 - codes
	union codeview pc = { .u8 = codes };
	for (; OFS() < size;) {
		printf("%5ld | ", OFS());
		enum OPCODE opcode  = *pc.u8;
		pc.u8		   += 1;

		switch (opcode) {
		case OP_CHAR:
			printf("CHAR %c", pc.u8[0]);
			pc.u8 += 1;
			break;
		case OP_JMP16:
			printf("JMP16 %d", pc.i16[0]);
			pc.u16 += 1;
			break;
		case OP_SPLIT8:
			printf("SPLIT8 %d %d", pc.i8[0], pc.i8[1]);
			pc.i8 += 2;
			break;
		case OP_SPLIT16:
			printf("SPLIT16 %d %d", pc.i16[0], pc.i16[1]);
			pc.i16 += 2;
			break;
		}

		printf("\n");
	}

#undef OFS
}

int main(int argc, const char **argv)
{
	struct sc_mem alloc = { .buf = NULL };

	if (argc != 2) {
		fprintf(stderr, "usage: regex <pattern>");
		goto FINISH;
	}

	struct {
		struct sb_mem *lexbuf;
		struct sb_mem *lexszbuf;
		struct sb_mem *typebuf;
		struct sb_mem *ofsbuf;
		const char   **lex;
		uint16_t      *lexsz;
		enum TKN      *types;
		uint32_t      *ofs;
		uint32_t       size;
	} tkns = {
		.lexbuf	  = sc_linbuf(&alloc),
		.lexszbuf = sc_linbuf(&alloc),
		.typebuf  = sc_linbuf(&alloc),
		.ofsbuf	  = sc_linbuf(&alloc),
	};

	const char   *pattern = argv[1];
	struct parser C1      = { .regex = pattern };

	while (next(&C1.regex, &C1.type, &C1.lexsz, &C1.lex) == RGX_OK) {
		tkns.lex   = sb_add(tkns.lexbuf, 0, sizeof(*tkns.lex));
		tkns.lexsz = sb_add(tkns.lexszbuf, 0, sizeof(*tkns.lexsz));
		tkns.types = sb_add(tkns.typebuf, 0, sizeof(*tkns.types));
		tkns.ofs   = sb_add(tkns.ofsbuf, 0, sizeof(*tkns.ofs));

		tkns.lex[tkns.size]    = C1.lex;
		tkns.lexsz[tkns.size]  = C1.lexsz;
		tkns.types[tkns.size]  = C1.type;
		tkns.ofs[tkns.size]    = C1.regex - pattern - C1.lexsz;
		tkns.size	      += 1;
	}

	printf("===================================="
	       "\n"
	       "|                                  |"
	       "\n"
	       "|           Regex Dump !           |"
	       "\n"
	       "|                                  |"
	       "\n"
	       "===================================="
	       "\n\n");

	printf("Tokens"
	       "\n"
	       "===================================="
	       "\n\n");

	if (tkns.size) {
		pr_tknlist(tkns.size, tkns.ofs, tkns.lexsz, tkns.lex,
			   tkns.types);
		printf("\n");
	}

	printf("Abstract Syntax Tree"
	       "\n"
	       "===================================="
	       "\n\n");

	struct rgxast asts   = { .size = 0 };
	const char   *errmsg = NULL;

	rgx_parse(&alloc, pattern, &asts, &errmsg);
	pr_astlist(&asts);

	if (errmsg != NULL)
		printf("\nerror: %s", errmsg);

	printf("\n\n");

	printf("VM Bytecode"
	       "\n"
	       "===================================="
	       "\n\n");

FINISH:
	sc_free(&alloc);
	return 0;
}
