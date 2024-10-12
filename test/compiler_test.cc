#include <gtest/gtest.h>

extern "C" {
#include "compiler.h"
}

static size_t read_file(const char *path, char **dst)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	uint32_t file_size = ftell(file);
	rewind(file);

	auto   buffer	  = (char *) malloc(file_size + 1);
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

TEST(CompilerTest, Basic)
{
	char  *src;
	size_t srcsz = read_file(BASIC_JARY_PATH, &src);

	struct jy_asts asts;
	struct jy_tkns tkns;
	struct jy_errs errs;
	struct jy_jay  ctx;

	memset(&asts, 0, sizeof(asts));
	memset(&tkns, 0, sizeof(tkns));
	memset(&errs, 0, sizeof(errs));
	memset(&ctx, 0, sizeof(ctx));

	ctx.mdir = "../modules/";

	jry_parse(src, srcsz, &asts, &tkns, &errs);
	jry_compile(&asts, &tkns, &ctx, &errs);

	// clang-format off
	uint8_t codes[] = {
                //
                JY_OP_PUSH8, 4,
                JY_OP_PUSH8, 5,
                JY_OP_EXACT,
                JY_OP_PUSH8, 6,
                JY_OP_QUERY,
                //
                JY_OP_PUSH8, 6,
                JY_OP_PUSH8, 7,
                JY_OP_CMP,
                JY_OP_JMPF,  8, 0,
                //
                JY_OP_PUSH8, 6,
                JY_OP_PUSH8, 8,
                JY_OP_GT,
                JY_OP_JMPF,  9, 0,
                //
                JY_OP_PUSH8, 10,
                JY_OP_PUSH8, 5,
                JY_OP_CALL,  1,
                JY_OP_END,
        };
	// clang-format on

	ASSERT_EQ(errs.size, 0);
	ASSERT_EQ(ctx.codesz, sizeof(codes));

	for (size_t i = 0; i < sizeof(codes); ++i)
		ASSERT_EQ(ctx.codes[i], codes[i]) << "codes[" << i << "]";

	jry_free_asts(asts);
	jry_free_tkns(tkns);
	jry_free_jay(ctx);
}
