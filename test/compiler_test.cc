#include <gtest/gtest.h>

extern "C" {
#include "compiler.h"
}

TEST(CompilerTest, Basic)
{
	const char src[] = "import mark"
			   "\n"
			   "\n"
			   "ingress data {"
			   "\n"
			   "       field:"
			   "\n"
			   "               yes = string"
			   "\n"
			   "               no = string"
			   "\n"
			   "}"
			   "\n"
			   "\n"
			   "rule bye {"
			   "\n"
			   "       match:"
			   "\n"
			   "                $data.yes = \"hello\""
			   "\n"
			   "       condition:"
			   "\n"
			   "               1 = 2 and 1 > 3"
			   "\n"
			   "       target:"
			   "\n"
			   "               mark.mark($data.yes)"
			   "\n"
			   "}";

	struct jy_asts asts;
	struct jy_tkns tkns;
	struct jy_errs errs;
	struct jy_defs names;
	struct jy_jay  ctx;

	memset(&asts, 0, sizeof(asts));
	memset(&tkns, 0, sizeof(tkns));
	memset(&errs, 0, sizeof(errs));
	memset(&names, 0, sizeof(names));
	memset(&ctx, 0, sizeof(ctx));

	ctx.names = &names;
	ctx.mdir  = "../modules/";

	jry_parse(src, sizeof(src), &asts, &tkns, &errs);
	jry_compile(&asts, &tkns, &ctx, &errs);

	// clang-format off
	uint8_t codes[] = {
                //
                JY_OP_PUSH8, 0,
                JY_OP_PUSH8, 1,
                JY_OP_CMP,
                JY_OP_JMPF,  25, 0,
                //
                JY_OP_PUSH8, 2,
                JY_OP_PUSH8, 3,
                JY_OP_CMP,
                JY_OP_JMPF,  8, 0,
                //
                JY_OP_PUSH8, 2,
                JY_OP_PUSH8, 4,
                JY_OP_GT,
                JY_OP_JMPF,  9, 0,
                //
                JY_OP_PUSH8, 0,
                JY_OP_CALL,  1, 0, 0,
                JY_OP_END,
        };
	// clang-format on

	ASSERT_EQ(errs.size, 0);
	ASSERT_EQ(ctx.codesz, sizeof(codes) / sizeof(codes[0]));
	ASSERT_EQ(memcmp(ctx.codes, codes, sizeof(codes)), 0);

	jry_free_asts(asts);
	jry_free_tkns(tkns);
	jry_free_errs(errs);
	jry_free_jay(ctx);
}
