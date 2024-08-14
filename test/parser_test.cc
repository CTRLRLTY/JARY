#include <gtest/gtest.h>
#include <vector>
#include <queue>

extern "C" {
#include "scanner.h"
#include "parser.h"
#include "vector.h"
}


// Breadth-First-Search ASTType matching 
void tree_match(ASTNode* ast, ASTMetadata* m, std::vector<ASTType>& types) {
    ASSERT_EQ(m->size, types.size());
    
    std::queue<ASTNode*> q;
    std::vector<bool> marked(m->size, false);

    q.push(ast);

    while (q.size() > 0) {
        ASTNode* node = q.front();
        q.pop();
        auto pos = node->position;

        if (!marked[pos]) {
            marked[pos] = true;
            ASSERT_EQ(node->type, types[pos]) << "node: " << node->type << ", expect: " << types[pos];

            for (size_t i = 0; i < ast_degree(node); i++) {
                auto neighbor = &node->child[i];
                auto npos = neighbor->position;

                if (!marked[npos]) {
                    q.push(neighbor);
                }
            }
        }
    }
    
}

TEST(ParserTest, RuleDeclaration) {
    Parser p;

    Scanner sc;
    jary_vec_t(TKN) tkns;

    {
        ASTNode ast = {};
        ASTMetadata m = {};
        jary_vec_init(tkns, 10);

        char samplestr[] = 
        "rule something {"              "\n"
            "input:"                    "\n"
            "got = myfunc(3)"           "\n"
            "corn = myfunc(4)"          "\n"
        "}"
        ;

        std::vector<ASTType> expected = {
            AST_ROOT, 
                AST_RULE, 
                    AST_NAME, 
                    AST_INPUT, 
                        AST_ASSIGNMENT,         // =
                            AST_NAME,           // got
                            AST_CALL,           // myfunc
                                AST_NAME,       // ()
                                AST_LITERAL,    // 3
                        AST_ASSIGNMENT,         // =
                            AST_NAME,           // corn
                            AST_CALL,           // ()
                                AST_NAME,       // myfunc
                                AST_LITERAL,    // 4
        };

        ASSERT_EQ(scan_source(&sc, samplestr, sizeof(samplestr)), SCAN_SUCCESS);

        while (!scan_ended(&sc)) {
            TKN tkn;
            ASSERT_EQ(scan_token(&sc, &tkn), SCAN_SUCCESS);
            jary_vec_push(tkns, tkn);
        }

        parse_tokens(&p, &ast, &m, tkns, jary_vec_size(tkns));

        tree_match(&ast, &m, expected);

        jary_vec_free(tkns);
    }
}