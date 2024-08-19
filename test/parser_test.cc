#include <gtest/gtest.h>
#include <vector>
#include <queue>

extern "C" {
#include "parser.h"
}


// Breadth-First-Search ASTType matching 
void tree_match(ASTNode* ast, ASTMetadata* m, std::vector<ASTType>& types) {
    EXPECT_EQ(m->size, types.size());
    
    std::queue<ASTNode*> q;
    std::vector<bool> marked(m->size, false);

    q.push(ast);

    size_t idx = 0;

    while (q.size() > 0) {
        ASTNode* node = q.front();
        q.pop();
        auto num = node->id;

        if (!marked[num]) {
            marked[num] = true;
            ASSERT_EQ(node->type, types[idx++]) << "nodenum: " << num;

            for (size_t i = 0; i < node->degree; i++) {
                auto neighbor = &node->child[i];
                auto nnum = neighbor->id;

                if (!marked[nnum]) {
                    q.push(neighbor);
                }
            }
        }
    }

    for (const auto& mark : marked) {
        ASSERT_EQ(mark, true);
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
                        AST_BINARY,             // =
                            AST_NAME,           // got
                            AST_CALL,           // myfunc
                            AST_NAME,           // corn
                            AST_CALL,           // ()
                                AST_NAME,       // ()
                                AST_LITERAL,    // 3
                                AST_NAME,       // myfunc
                                AST_LITERAL,    // 4
        };

        jary_parse(&ast, &m, samplestr, sizeof(samplestr));

        EXPECT_EQ(m.errsz, 0);

        tree_match(&ast, &m, expected);

        ast_free(&ast);
        ast_meta_free(&m);
    }

    {
        ASTNode ast = {};
        ASTMetadata m = {};

        char samplestr[] = 
        "rule something {"              "\n"
            "input:"                    "\n"
            "got = myfunc(3"           "\n"
        "}"
        ;

        std::vector<ASTType> expected = {
            AST_ROOT, 
                AST_DECL,                       // rule
                    AST_NAME,                   // something
                    AST_SECTION,                // input:
        };

        jary_parse(&ast, &m, samplestr, sizeof(samplestr));

        tree_match(&ast, &m, expected);

        ASSERT_EQ(m.errsz, 1);

        ast_free(&ast);
        ast_meta_free(&m);
    }

    {
        ASTNode ast = {};
        ASTMetadata m = {};

        char samplestr[] = 
        "rule something {"              "\n" // 3
            "input:"                    "\n" // 1
                "$got = myfunc(3)"      "\n" // $ got = myfunc () 3
        "}"
        ;

        std::vector<ASTType> expected = {
            AST_ROOT, 
                AST_DECL,                       // rule
                    AST_NAME,                   // something
                    AST_SECTION,                // input:
                        AST_BINARY,             // =
                            AST_EVENT,          // $
                            AST_CALL,           // ()
                                AST_NAME,       // got
                                AST_NAME,       // myfunc
                                AST_LITERAL,    // 3
        };

        jary_parse(&ast, &m, samplestr, sizeof(samplestr));

        tree_match(&ast, &m, expected);

        ast_free(&ast);
        ast_meta_free(&m);
    }
}
