#include <gtest/gtest.h>
#include <vector>
#include <queue>

extern "C" {
#include "parser.h"
}

static void treematch(ASTNode* ast, std::vector<ASTType>* types, size_t index) {
    ASTType type = (*types)[index];

    ASSERT_EQ(ast->type, type) << "index: " << index;

    for (size_t i = 1; i < ast->degree; ++i) {
        ASTNode v = ast->child[i-1];

        treematch(&v, types, index + i);
    }
}

TEST(ParserTest, RuleDeclaration) {
    {
        ASTNode ast;
        ASTMetadata m;

        char samplestr[] = 
        "rule something {"              "\n"
            "input:"                    "\n"
            "got = myfunc(3)"           "\n"
            "corn = myfunc(4)"          "\n"
        "}"
        ;

        std::vector<ASTType> expected = {
            AST_ROOT, 
                AST_DECL,                       // rule
                    AST_NAME,                   // something
                    AST_SECTION,                // input:
                        AST_BINARY,             // =
                            AST_NAME,           // got
                            AST_CALL,           // (
                                AST_NAME,       // myfunc
                                AST_LITERAL,    // 3
                        AST_BINARY,             // =
                            AST_NAME,           // corn
                            AST_CALL,           // (
                                AST_NAME,       // but
                                AST_LITERAL,    // 3
        };

        jary_parse(&ast, &m, samplestr, sizeof(samplestr));

        EXPECT_EQ(m.errsz, 0);

        treematch(&ast, &expected, 0);

        free_ast(&ast);
        free_ast_meta(&m);
    }
}
