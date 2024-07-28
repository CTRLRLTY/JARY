#include <gtest/gtest.h>

extern "C" {
#include "scanner.h"
#include "parser.h"
}

TEST(ParserTest, RuleDeclaration) {
    Parser p;

    Scanner sc;
    Vector tkns;
    ASTProg prog;

    {
        ASSERT_EQ(vec_init(&tkns, sizeof(TKN)), VEC_SUCCESS);

        char samplestr[] = 
        "rule something"
        "{\n"
            "input:\n"
            "$got = myfunc(3)\n"
        "}";

        ASSERT_EQ(scan_source(&sc, samplestr), SCAN_SUCCESS);

        while (!sc.ended) {
            TKN tkn;
            ASSERT_EQ(scan_token(&sc, &tkn), SCAN_SUCCESS);
            ASSERT_EQ(vec_push(&tkns, &tkn, sizeof(tkn)), VEC_SUCCESS);
        }

        ASSERT_EQ(parse_init(&p), PARSE_SUCCESS);
        ASSERT_EQ(ASTProg_init(&prog), true);
        ASSERT_EQ(parse_tokens(&p, &tkns, &prog), PARSE_SUCCESS);
        ASSERT_EQ(ASTProg_free(&prog), true);
    }
}