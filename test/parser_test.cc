#include <gtest/gtest.h>

extern "C" {
#include "parser.h"
}

TEST(ParserTest, ImportStatement)
{
	const char src[] = "import mark";

	struct jy_asts asts;
	struct jy_tkns tkns;
	struct jy_errs errs;

	memset(&asts, 0, sizeof(asts));
	memset(&tkns, 0, sizeof(tkns));
	memset(&errs, 0, sizeof(errs));

	jry_parse(src, sizeof(src), &asts, &tkns, &errs);

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

	jry_free_asts(asts);
	jry_free_tkns(tkns);
	jry_free_errs(errs);
}

TEST(ParserTest, IngressDeclaration)
{
	const char src[] = "ingress data {"
			   "\n"
			   "      field:"
			   "\n"
			   "            yes == string"
			   "\n"
			   "            no == long"
			   "\n"
			   "}";

	struct jy_asts asts;
	struct jy_tkns tkns;
	struct jy_errs errs;

	memset(&asts, 0, sizeof(asts));
	memset(&tkns, 0, sizeof(tkns));
	memset(&errs, 0, sizeof(errs));

	jry_parse(src, sizeof(src), &asts, &tkns, &errs);

	enum jy_ast a[] = {
		AST_ROOT,
		AST_INGRESS_DECL,
		AST_FIELD_SECT,
		AST_EVENT_MEMBER_NAME,
		AST_EVENT_MEMBER_DECL,
		AST_STR_TYPE,
		AST_EVENT_MEMBER_NAME,
		AST_EVENT_MEMBER_DECL,
		AST_LONG_TYPE,
	};

	ASSERT_EQ(errs.size, 0);
	ASSERT_EQ(asts.size, sizeof(a) / sizeof(a[0]));
	ASSERT_EQ(memcmp(asts.types, a, sizeof(a)), 0);

	jry_free_asts(asts);
	jry_free_tkns(tkns);
	jry_free_errs(errs);
}

TEST(ParserTest, RuleDeclaration)
{
	const char src[] = "rule something {"
			   "\n"
			   "      match:"
			   "\n"
			   "              $data.yes <=> \"hello\" "
			   "\n"
			   "              $data.num <=> 3 "
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

	struct jy_asts asts;
	struct jy_tkns tkns;
	struct jy_errs errs;

	memset(&asts, 0, sizeof(asts));
	memset(&tkns, 0, sizeof(tkns));
	memset(&errs, 0, sizeof(errs));

	jry_parse(src, sizeof(src), &asts, &tkns, &errs);

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

	jry_free_asts(asts);
	jry_free_tkns(tkns);
	jry_free_errs(errs);
}
