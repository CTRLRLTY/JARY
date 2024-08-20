#include <string>
#include <vector>
#include <cstring>

#include <gtest/gtest.h>

extern "C" {
#include "scanner.h"
#include "fnv.h"
}

TEST(ScannerTest, ScanEOF) {
  Scanner sc;
  Tkn token;

  char samplestr[] = "\0\0";

  scan_source(&sc, samplestr, sizeof(samplestr));

  scan_token(&sc, &token);

  ASSERT_EQ(token.length, 3);

  ASSERT_STREQ(token.start, samplestr);

  ASSERT_EQ(scan_ended(&sc), true);
}

TEST(ScannerTest, ScanNewline) {
  Scanner sc;
  Tkn token;

  char samplestr[] = "\n\n\n\n\n";

  scan_source(&sc, samplestr, sizeof(samplestr));

  scan_token(&sc, &token);

  EXPECT_EQ(token.type, TKN_NEWLINE);

  char buf[lexsize(&token)];

  ASSERT_EQ(lexemestr(&token, buf, sizeof(buf)), true);

  scan_token(&sc, &token);

  ASSERT_EQ(token.type, TKN_EOF);
}

TEST(ScannerTest, ScanWhitespace) {
  Scanner sc;
  Tkn token;

  { // check each whitespace character
    char *spaces[] = {
      " ",
      "\t",
      "\r",
    };

    size_t spsz = sizeof(spaces)/sizeof(spaces[0]);

    for (size_t i = 0; i < spsz; ++i) {
      scan_source(&sc, spaces[i], strlen(spaces[i]));
      
      scan_token(&sc, &token);

      EXPECT_EQ(token.type, TKN_EOF) << "i: " << i;

      EXPECT_EQ(scan_ended(&sc), true) << "i: " << i;
    }
  }

  { // consecutive whitespaces
    char samplestr[] = "    \t\t\r\t  \r";

    scan_source(&sc, samplestr, sizeof(samplestr));
      
    scan_token(&sc, &token);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(scan_ended(&sc), true);
  }
}

TEST(ScannerTest, ScanSymbol) {
  Scanner sc;
  Tkn token;

  struct {
    char* cstr;
    TknType type;
  } symbols[] = {
    {"(", TKN_LEFT_PAREN}, {")", TKN_RIGHT_PAREN},
    {"{", TKN_LEFT_BRACE}, {"}", TKN_RIGHT_BRACE},
    {"=", TKN_EQUAL}, {"~", TKN_TILDE},
    {"<", TKN_LESSTHAN}, {">", TKN_GREATERTHAN},
    {":", TKN_COLON},
    {",", TKN_COMMA}, 
    {"$", TKN_DOLLAR},
  };

  size_t symsz = sizeof(symbols)/sizeof(symbols[0]);

  for (size_t i = 0; i < symsz; ++i) {
    scan_source(&sc, symbols[i].cstr, 2);
    
    scan_token(&sc, &token);

    EXPECT_EQ(token.type, symbols[i].type) << "symbol: " << symbols[i].cstr;

    EXPECT_STREQ(token.start, symbols[i].cstr);

    scan_token(&sc, &token);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(scan_ended(&sc), true);
  }
}

TEST(ScannerTest, ScanKeyword) {
  Scanner sc;
  Tkn token;

  struct {
    char* cstr;
    size_t size;
    TknType type;
  } keywords[] = {
    {"all", 4, TKN_ALL},
    {"and", 4, TKN_AND},
    {"any", 4,TKN_ANY},
    {"false", 6, TKN_FALSE}, 
    {"true", 5, TKN_TRUE},
    {"or", 3, TKN_OR},
    {"not", 4, TKN_NOT},
    {"input", 6, TKN_INPUT},
    {"rule", 5, TKN_RULE},
    {"import", 7, TKN_IMPORT},
    {"ingress", 8, TKN_INGRESS},
    {"include", 8, TKN_INCLUDE},
    {"match", 6, TKN_MATCH},
    {"target", 7, TKN_TARGET},
    {"condition", 10, TKN_CONDITION},
    {"fields", 7, TKN_FIELDS},
  };

  size_t kwsz = sizeof(keywords)/sizeof(keywords[0]);

  for (size_t i = 0; i < kwsz; ++i) { // Check every keyword
    scan_source(&sc, keywords[i].cstr, keywords[i].size);
    
    scan_token(&sc, &token);

    EXPECT_EQ(token.type, keywords[i].type) << "keyword: " << keywords[i].cstr;

    EXPECT_STREQ(token.start, keywords[i].cstr);

    scan_token(&sc, &token);

    ASSERT_EQ(token.type, TKN_EOF) << keywords[i].cstr;

    ASSERT_EQ(scan_ended(&sc), true);
  }
}

TEST(ScannerTest, ScanString) {
  Scanner sc;
  Tkn token;

  { // Basic string test
    char samplestr[] = "\"Hello world\"";

    scan_source(&sc, samplestr, sizeof(samplestr));

    scan_token(&sc, &token);

    EXPECT_EQ(token.type, TKN_STRING);

    EXPECT_STREQ(token.start, samplestr);

    EXPECT_EQ(lexsize(&token), sizeof(samplestr)); 

    scan_token(&sc, &token);

    ASSERT_EQ(token.type, TKN_EOF);

    scan_ended(&sc);
  }
  
  { // Multiline string
    char samplestr[] = "\"Hello \
                         world\"";
    
    scan_source(&sc, samplestr, sizeof(samplestr));

    scan_token(&sc, &token);

    EXPECT_EQ(token.type, TKN_STRING);

    EXPECT_STREQ(token.start, samplestr);

    EXPECT_EQ(lexsize(&token), sizeof(samplestr));

    scan_token(&sc, &token);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(scan_ended(&sc), true);
  }

  { // Unterminated string 
    char samplestr[] = "\"Unterminated String";
         
    scan_source(&sc, samplestr, sizeof(samplestr));

    scan_token(&sc, &token);

    EXPECT_EQ(token.type, TKN_ERR_STR);
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

    scan_source(&sc, samplestr, sizeof(samplestr));

    for (uint8_t i = 0; i < 3; ++i) {
      char* str = str1 + sizeof(str1) * i;

      scan_token(&sc, &token);

      size_t lexsz = lexsize(&token);

      EXPECT_EQ(token.type, TKN_STRING);

      EXPECT_EQ(lexsz, sizeof(str1)) << "string: " << str;

      char buf[lexsz];

      ASSERT_EQ(lexemestr(&token, buf, sizeof(buf)), true) << "string: " << str;

      EXPECT_STREQ(buf, str);

      ASSERT_EQ(scan_ended(&sc), false);
    }
    
    scan_token(&sc, &token);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(scan_ended(&sc), true);
  }
}

TEST(ScannerTest, ScanRegexp) {
  Scanner sc;
  Tkn token;

  { // Basic regexp
    char samplestr[] = "/Hello world\\//";

    scan_source(&sc, samplestr, sizeof(samplestr));

    scan_token(&sc, &token);

    EXPECT_EQ(token.type, TKN_REGEXP);

    EXPECT_STREQ(token.start, samplestr);

    EXPECT_EQ(lexsize(&token), sizeof(samplestr)); 

    scan_token(&sc, &token);

    ASSERT_EQ(token.type, TKN_EOF);

    ASSERT_EQ(scan_ended(&sc), true);
  }
}

TEST(ScannerTest, ScanIdentifier) {
  Scanner sc;
  Tkn token;

  { // legal identifiers
    std::vector<std::string> ident = {
      "hello",
      "_my_name"
      "identifier123",
    };

    for (size_t i = 0; i < ident.size(); ++i) {
      char str[ident[i].size() + 1] = "\0";
      
      std::strcpy(str, ident[i].c_str());
      
      scan_source(&sc, str, sizeof(str));

      scan_token(&sc, &token);

      EXPECT_EQ(token.type, TKN_IDENTIFIER) << "identifier: " << str;

      EXPECT_STREQ(token.start, str);

      scan_token(&sc, &token);

      EXPECT_EQ(token.type, TKN_EOF);

      EXPECT_EQ(scan_ended(&sc), true);
    }
  }

  { // illegal identifier
    char samplestr[] = "1hello";
      
    scan_source(&sc, samplestr, sizeof(samplestr));

    scan_token(&sc, &token);

    EXPECT_EQ(token.type, TKN_NUMBER);

    EXPECT_EQ(scan_ended(&sc), false);
  }
}