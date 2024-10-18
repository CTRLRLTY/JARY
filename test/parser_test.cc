
#include <gtest/gtest.h>

extern "C" {
#include "parser.h"

#include "jary/memory.h"
}

TEST(ParserTest, ImportStatement)
{
	const char src[] = "import mark";

	struct jy_asts asts  = { .tkns = NULL };
	struct jy_tkns tkns  = { .lexemes = NULL };
	struct jy_errs errs  = { .msgs = NULL };
	struct sc_mem  alloc = { .buf = NULL };

	jry_parse(&alloc, &asts, &tkns, &errs, src, sizeof(src));

	enum jy_ast a[] = {
		AST_ROOT,
		AST_IMPORT_STMT,
	};

	enum jy_tkn t[] = {
		TKN_NONE, TKN_IMPORT, TKN_SPACES, TKN_IDENTIFIER, TKN_EOF,
	};

	ASSERT_EQ(errs.size, 0);
	ASSERT_EQ(tkns.size, sizeof(t) / sizeof(t[0]));
	ASSERT_EQ(asts.size, sizeof(a) / sizeof(a[0]));
	ASSERT_EQ(memcmp(asts.types, a, asts.size * sizeof(a[0])), 0);
	ASSERT_EQ(memcmp(tkns.types, t, tkns.size * sizeof(t[0])), 0);

	sc_free(&alloc);
}

TEST(ParserTest, IngressDeclaration)
{
	const char src[] = "ingress data {"
			   "\n"
			   "      field:"
			   "\n"
			   "            yes string"
			   "\n"
			   "            no long"
			   "\n"
			   "}";

	struct jy_asts asts  = { .tkns = NULL };
	struct jy_tkns tkns  = { .lexemes = NULL };
	struct jy_errs errs  = { .msgs = NULL };
	struct sc_mem  alloc = { .buf = NULL };

	jry_parse(&alloc, &asts, &tkns, &errs, src, sizeof(src));

	enum jy_ast a[] = {
		AST_ROOT,	  AST_INGRESS_DECL, AST_FIELD_SECT,
		AST_EVENT_MEMBER, AST_STR_TYPE,	    AST_EVENT_MEMBER,
		AST_LONG_TYPE,
	};

	ASSERT_EQ(errs.size, 0);
	ASSERT_EQ(asts.size, sizeof(a) / sizeof(a[0]));
	ASSERT_EQ(memcmp(asts.types, a, sizeof(a)), 0);

	sc_free(&alloc);
}

TEST(ParserTest, RuleDeclaration)
{
	const char src[] = "rule something {"
			   "\n"
			   "      match:"
			   "\n"
			   "              $data.yes exact \"hello\" "
			   "\n"
			   "              $data.num exact 3 "
			   "\n"
			   "      condition:"
			   "\n"
			   "              1 == 2 or 1 > 3"
			   "\n"
			   "              1 < 2 and \"a\" == \"a\""
			   "\n"
			   "      target:"
			   "\n"
			   "              mark.mark($data.yes)"
			   "\n"
			   "}";

	struct jy_asts asts  = { .tkns = NULL };
	struct jy_tkns tkns  = { .lexemes = NULL };
	struct jy_errs errs  = { .msgs = NULL };
	struct sc_mem  alloc = { .buf = NULL };

	jry_parse(&alloc, &asts, &tkns, &errs, src, sizeof(src));

	enum jy_ast expect[] = {
		AST_ROOT,     AST_RULE_DECL,	  AST_MATCH_SECT,
		AST_EVENT,    AST_ACCESS,	  AST_EVENT,
		AST_EXACT,    AST_STRING,	  AST_EVENT,
		AST_ACCESS,   AST_EVENT,	  AST_EXACT,
		AST_LONG,     AST_CONDITION_SECT, AST_LONG,
		AST_EQUALITY, AST_LONG,		  AST_OR,
		AST_LONG,     AST_GREATER,	  AST_LONG,
		AST_LONG,     AST_LESSER,	  AST_LONG,
		AST_AND,      AST_STRING,	  AST_EQUALITY,
		AST_STRING,   AST_JUMP_SECT,	  AST_NAME,
		AST_ACCESS,   AST_NAME,		  AST_CALL,
		AST_EVENT,    AST_ACCESS,	  AST_EVENT
	};

	ASSERT_EQ(errs.size, 0);

	ASSERT_EQ(asts.size, sizeof(expect) / sizeof(expect[0]));
	ASSERT_EQ(memcmp(asts.types, expect, asts.size * sizeof(expect[0])), 0);

	sc_free(&alloc);
}
