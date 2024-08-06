#include <gtest/gtest.h>

extern "C" {
#include "scanner.h"
#include "parser.h"
}

TEST(ParserTest, RuleDeclaration) {
    Parser p;

    Scanner sc;
    jary_vec_t(TKN) tkns;
    ASTProg prog;

    {
        jary_vec_init(tkns, 10);

        char samplestr[] = 
        "rule something"
        "{\n"
            "input:\n"
            "$got = myfunc(3)\n"
        "}";

        ASSERT_EQ(scan_source(&sc, samplestr, sizeof(samplestr)), SCAN_SUCCESS);

        while (!scan_ended(&sc)) {
            TKN tkn;
            ASSERT_EQ(scan_token(&sc, &tkn), SCAN_SUCCESS);
            jary_vec_push(tkns, tkn);
        }

        ASSERT_EQ(parse_source(&p, tkns, jary_vec_size(tkns)), PARSE_SUCCESS);
        
        while (!parse_ended(&p)) {
            ParsedAst ast;
            ParsedType type;
            ASSERT_EQ(parse_tokens(&p, &ast, &type), PARSE_SUCCESS);
            ASSERT_EQ(type, PARSED_RULE);
        }

        jary_vec_free(tkns);
    }
}