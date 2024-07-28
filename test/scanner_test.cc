#include <gtest/gtest.h>
#include <string.h>

extern "C" {
#include "scanner.h"
#include "fnv.h"
}

TEST(ScannerTest, ScanInitFree) {
  Scanner sc;

  {
    ASSERT_EQ(scan_init(&sc), SCAN_SUCCESS);
    ASSERT_EQ(scan_free(&sc), SCAN_SUCCESS);
  }

  {
    ASSERT_EQ(scan_init(&sc), SCAN_SUCCESS);
    ASSERT_EQ(scan_add_name(&sc, "something", sizeof("something")), SCAN_SUCCESS);
    ASSERT_EQ(scan_free(&sc), SCAN_SUCCESS);
    EXPECT_EQ(sc.thash.data, nullptr);
  }
}

TEST(ScannerTest, ScanEOF) {
  Scanner sc;
  TKN token;

  char samplestr[] = "\0\0";

  ASSERT_EQ(scan_source(&sc, samplestr), SCAN_SUCCESS);

  ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS);

  ASSERT_EQ(token.length, 1);

  ASSERT_STREQ(token.start, "\0");

  ASSERT_EQ(sc.ended, true);

  ASSERT_EQ(scan_token(&sc, &token), ERR_SCAN_ENDED);
}

TEST(ScannerTest, ScanNewline) {
  Scanner sc;
  TKN token;

  char samplestr[] = "\n\n\n\n\n";

  ASSERT_EQ(scan_source(&sc, samplestr), SCAN_SUCCESS);

  ASSERT_EQ(scan_token(&sc, AS_PTKN(token)), SCAN_SUCCESS);

  EXPECT_EQ(token.type, TKN_NEWLINE);

  char buf[tkn_lexeme_size(AS_PTKN(token))];

  ASSERT_EQ(tkn_lexeme(AS_PTKN(token), buf, sizeof(buf)), true);

  ASSERT_EQ(scan_token(&sc, AS_PTKN(token)), SCAN_SUCCESS);

  ASSERT_EQ(token.type, TKN_EOF);
}

TEST(ScannerTest, ScanWhitespace) {
  Scanner sc;
  TKN token;

  { // check each whitespace character
    char *spaces[] = {
      " ",
      "\t",
      "\r",
    };

    size_t spsz = sizeof(spaces)/sizeof(spaces[0]);

    for (size_t i = 0; i < spsz; ++i) {
      ASSERT_EQ(scan_source(&sc, spaces[i]), SCAN_SUCCESS) << "i: " << i;
      
      ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS) << "i: " << i;

      EXPECT_EQ(token.type, TKN_EOF) << "i: " << i;

      EXPECT_EQ(sc.ended, true) << "i: " << i;
    }
  }

  { // consecutive whitespaces
    char samplestr[] = "    \t\t\r\t  \r";

    ASSERT_EQ(scan_source(&sc, samplestr), SCAN_SUCCESS);
      
    ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(sc.ended, true);
  }
}

TEST(ScannerTest, ScanSymbol) {
  Scanner sc;
  TKN token;

  struct {
    char* cstr;
    TknType type;
  } symbols[] = {
    {"(", TKN_LEFT_PAREN}, {")", TKN_RIGHT_PAREN},
    {"{", TKN_LEFT_BRACE}, {"}", TKN_RIGHT_BRACE},
    {"<", TKN_LEFT_PBRACE}, {">", TKN_RIGHT_PBRACE},
    {":", TKN_COLON},
    {",", TKN_COMMA},
  };

  size_t symsz = sizeof(symbols)/sizeof(symbols[0]);

  for (size_t i = 0; i < symsz; ++i) {
    ASSERT_EQ(scan_source(&sc, symbols[i].cstr), SCAN_SUCCESS) << "symbol: " << symbols[i].cstr;
    
    ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS) << "symbol: " << symbols[i].cstr;

    EXPECT_EQ(token.type, symbols[i].type) << "symbol: " << symbols[i].cstr;

    EXPECT_STREQ(token.start, symbols[i].cstr);

    ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(sc.ended, true);
  }
}

TEST(ScannerTest, ScanKeyword) {
  Scanner sc;
  TKN token;

  struct {
    char* cstr;
    TknType type;
  } keywords[] = {
    {"all", TKN_ALL},
    {"and", TKN_AND},
    {"any", TKN_ANY},
    {"false", TKN_FALSE}, 
    {"true", TKN_TRUE},
    {"or", TKN_OR},
    {"input", TKN_INPUT},
    {"rule", TKN_RULE},
    {"match", TKN_MATCH},
    {"condition", TKN_CONDITION},
    {"$myvar", TKN_PVAR},
  };

  size_t kwsz = sizeof(keywords)/sizeof(keywords[0]);

  for (size_t i = 0; i < kwsz; ++i) { // Check every keyword
    ASSERT_EQ(scan_source(&sc, keywords[i].cstr), SCAN_SUCCESS) << "keyword: " << keywords[i].cstr;
    
    ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS) << "keyword: " << keywords[i].cstr;

    EXPECT_EQ(token.type, keywords[i].type) << "keyword: " << keywords[i].cstr;

    EXPECT_STREQ(token.start, keywords[i].cstr);

    ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(sc.ended, true);
  }
}

TEST(ScannerTest, ScanString) {
  Scanner sc;
  TKN token;

  { // Basic string test
    char samplestr[] = "\"Hello world\"";

    ASSERT_EQ(scan_source(&sc, samplestr), SCAN_SUCCESS);

    ASSERT_EQ(scan_token(&sc, AS_PTKN(token)), SCAN_SUCCESS);

    EXPECT_EQ(token.type, TKN_STRING);

    EXPECT_STREQ(token.start, samplestr);

    EXPECT_EQ(tkn_lexeme_size(AS_PTKN(token)), sizeof(samplestr)); 

    ASSERT_EQ(scan_token(&sc, AS_PTKN(token)), SCAN_SUCCESS);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(sc.ended, true);
  }
  
  { // Multiline string
    char samplestr[] = "\"Hello \
                         world\"";
    
    ASSERT_EQ(scan_source(&sc, samplestr), SCAN_SUCCESS);

    ASSERT_EQ(scan_token(&sc, AS_PTKN(token)), SCAN_SUCCESS);

    EXPECT_EQ(token.type, TKN_STRING);

    EXPECT_STREQ(token.start, samplestr);

    EXPECT_EQ(tkn_lexeme_size(AS_PTKN(token)), sizeof(samplestr));

    ASSERT_EQ(scan_token(&sc, AS_PTKN(token)), SCAN_SUCCESS);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(sc.ended, true);
  }

  { // Unterminated string 
    char samplestr[] = "\"Unterminated String";
         
    ASSERT_EQ(scan_source(&sc, samplestr), SCAN_SUCCESS);

    ASSERT_EQ(scan_token(&sc, &token), ERR_SCAN_INV_STRING);

    EXPECT_EQ(token.type, TKN_ERR);

    ASSERT_EQ(sc.ended, true);
  }

  { // Consecutive strings
    char str1[] = "\"String 1\"";
    char str2[] = "\"String 2\"";
    char str3[] = "\"String 3\"";

    char samplestr[sizeof(str1)+sizeof(str2)+sizeof(str3)-2]; // -2 to not account str1 and str2 '\0'

    strcpy(samplestr, str1);
    strcat(samplestr, str2);
    strcat(samplestr, str3);

    // Making sure the sample strings have same length
    ASSERT_EQ(sizeof(str1) == sizeof(str2) && sizeof(str2) == sizeof(str3), true);

    ASSERT_EQ(scan_source(&sc, samplestr), SCAN_SUCCESS);

    for (uint8_t i = 0; i < 3; ++i) {
      char* str = str1 + sizeof(str1) * i;
      ASSERT_EQ(scan_token(&sc, AS_PTKN(token)), SCAN_SUCCESS) << "string: " << str;

      EXPECT_EQ(token.type, TKN_STRING);

      EXPECT_EQ(tkn_lexeme_size(AS_PTKN(token)), sizeof(str1)) << "string: " << str;

      char buf[tkn_lexeme_size(AS_PTKN(token))];

      ASSERT_EQ(tkn_lexeme(AS_PTKN(token), buf, sizeof(buf)), true) << "string: " << str;

      EXPECT_STREQ(buf, str);

      ASSERT_EQ(sc.ended, false);
    }
    
    ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(sc.ended, true);
  }
}

TEST(ScannerTest, ScanIdentifier) {
  Scanner sc;
  TKN token;

  { // legal identifiers
    char* identifiers[] = {
      "hello",
      "_myname",
      "identifier123",
      "my_naHe5555509_5",
      "___",
      "_m_m",
    };

    size_t idsz = sizeof(identifiers)/sizeof(identifiers[0]);

    for (size_t i = 0; i < idsz; ++i) {
      char* str = identifiers[i];
      
      ASSERT_EQ(scan_source(&sc, str), SCAN_SUCCESS) << "identifier: " << identifiers[i];

      ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS) << "identifier: " << identifiers[i];

      EXPECT_EQ(token.type, TKN_IDENTIFIER) << "identifier: " << identifiers[i];

      EXPECT_STREQ(token.start, str);

      ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS) << "identifier: " << identifiers[i];

      EXPECT_EQ(token.type, TKN_EOF);

      EXPECT_EQ(sc.ended, true);
    }
  }

  { // illegal identifier
    char samplestr[] = "1hello";
      
    ASSERT_EQ(scan_source(&sc, samplestr), SCAN_SUCCESS);

    ASSERT_EQ(scan_token(&sc, &token), SCAN_SUCCESS);

    EXPECT_EQ(token.type, TKN_NUMBER);

    EXPECT_EQ(sc.ended, false);
  }
}

#ifdef FEATURE_CUSTOM_TOKEN
TEST(ScannerTest, CustomToken) {
  Scanner sc;
  TKN tkn;

  { // single tkn
    char newtkn[] = "mytoken";

    ASSERT_EQ(scan_init(&sc), SCAN_SUCCESS);
    
    ASSERT_EQ(scan_add_name(&sc, newtkn, sizeof(newtkn)), SCAN_SUCCESS);
    
    EXPECT_EQ(sc.allowtknc, true);

    EXPECT_EQ(scan_source(&sc, newtkn), SCAN_SUCCESS);

    EXPECT_EQ(scan_token(&sc, AS_PTKN(tkn)), SCAN_SUCCESS);

    EXPECT_EQ(tkn.type, TKN_CUSTOM);

    char buf[tkn_lexeme_size(AS_PTKN(tkn))];

    ASSERT_EQ(tkn_lexeme(AS_PTKN(tkn), buf, sizeof(buf)), true);

    EXPECT_STREQ(buf, newtkn);
    
    EXPECT_EQ(tkn.hash, fnv_hash(newtkn, sizeof(newtkn)));

    ASSERT_EQ(scan_free(&sc), SCAN_SUCCESS);

    ASSERT_EQ(sc.thash.data, nullptr);
  }
}
#endif // FEATURE_CUSTOM_TOKEN