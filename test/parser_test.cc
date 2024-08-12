#include <gtest/gtest.h>

extern "C" {
#include "scanner.h"
#include "parser.h"
}

TEST(ParserTest, RuleDeclaration) {
    Parser p;

    Scanner sc;
    jary_vec_t(TKN) tkns;

    {
        ASTNode ast;
        jary_vec_init(tkns, 10);

        char samplestr[] = 
        "rule something {"              "\n"
            "input:"                    "\n"
            "got = myfunc(3)"           "\n"
            "got = myfunc(3)"           "\n"
        "}"
        ;

        ASSERT_EQ(scan_source(&sc, samplestr, sizeof(samplestr)), SCAN_SUCCESS);

        while (!scan_ended(&sc)) {
            TKN tkn;
            ASSERT_EQ(scan_token(&sc, &tkn), SCAN_SUCCESS);
            jary_vec_push(tkns, tkn);
        }

        parse_tokens(&p, &ast, tkns, jary_vec_size(tkns));

        jary_vec_free(tkns);
    }
}