extern "C" {
#include "parser.h"
}

#include <gtest/gtest.h>

#include <vector>

static void treematch(ASTNode* ast, std::vector<enum jy_ast>* types,
                      size_t index) {
  enum jy_ast type = (*types)[index];

  ASSERT_EQ(ast->type, type) << "index: " << index;

  for (size_t i = 1; i < ast->degree; ++i) {
    ASTNode v = ast->child[i - 1];

    treematch(&v, types, index + i);
  }
}

TEST(ParserTest, RuleDeclaration) {
  {
    char samplestr[] =
        "rule something {"
        "\n"
        "input:"
        "\n"
        "got = myfunc(3)"
        "\n"
        "corn = myfunc(4)"
        "\n"
        "}";

    std::vector<enum jy_ast> expected = {
        AST_ROOT,
        AST_DECL,     // rule
        AST_NAME,     // something
        AST_SECTION,  // input:
        AST_BINARY,   // =
        AST_NAME,     // got
        AST_CALL,     // (
        AST_NAME,     // myfunc
        AST_LITERAL,  // 3
        AST_BINARY,   // =
        AST_NAME,     // corn
        AST_CALL,     // (
        AST_NAME,     // but
        AST_LITERAL,  // 3
    };

    jry_parse(&ast, &m, samplestr, sizeof(samplestr));

    EXPECT_EQ(m.errsz, 0);

    treematch(&ast, &expected, 0);

    free_ast(&ast);
    free_ast_meta(&m);
  }
}
