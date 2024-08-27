#include <string>
#include <vector>
#include <cstring>

#include <gtest/gtest.h>

extern "C" {
#include "scanner.h"
#include "fnv.h"
#include "memory.h"
}

// TEST(ScannerTest, ScanEOF) {
//   char samplestr[] = "\0\0";

//   enum jy_tkn type;
//   size_t line = 1;
//   size_t ofs = 0;
//   size_t len = sizeof(samplestr);
//   char* lexeme = NULL;
//   size_t read = jry_scan(samplestr, len, &type, &line, &ofs, &lexeme);

//   ASSERT_EQ(read, 0);
//   ASSERT_EQ(line, 1);

//   ASSERT_EQ(type, TKN_EOF);
// }

// TEST(ScannerTest, ScanNewline) {
//   char samplestr[] = "\n\n\n\n\n";

//   enum jy_tkn type;
//   size_t line = 1;
//   size_t ofs = 0;
//   size_t len = sizeof(samplestr);
//   char* lexeme = NULL;
//   size_t read = jry_scan(samplestr, len, &type, &line, &ofs, &lexeme);

//   ASSERT_EQ(read, 5);
//   ASSERT_EQ(type, TKN_NEWLINE);
//   ASSERT_EQ(line, 6);
//   ASSERT_STREQ(lexeme, samplestr);
//   jry_free(lexeme);
// }

// TEST(ScannerTest, ScanWhitespace) {
//   enum jy_tkn type;
//   size_t line = 1;
//   char* lexeme = NULL;
//   char samplestr[] = "    \t\t\r\t  \r";
//   size_t ofs = 0;
//   size_t len = sizeof(samplestr);

//   size_t read = jry_scan(samplestr, len, &type, &line, &ofs, &lexeme);
  
//   ASSERT_EQ(read, 12);
//   ASSERT_EQ(line, 1);
//   ASSERT_EQ(type, TKN_EOF);
// }

// TEST(ScannerTest, ScanSymbol) {
//   struct {
//     char* cstr;
//     enum jy_tkn type;
//   } symbols[] = {
//     {"(", TKN_LEFT_PAREN}, {")", TKN_RIGHT_PAREN},
//     {"{", TKN_LEFT_BRACE}, {"}", TKN_RIGHT_BRACE},
//     {"=", TKN_EQUAL}, {"~", TKN_TILDE},
//     {"<", TKN_LESSTHAN}, {">", TKN_GREATERTHAN},
//     {":", TKN_COLON},
//     {",", TKN_COMMA}, 
//     {"$", TKN_DOLLAR},
//   };

//   size_t symsz = sizeof(symbols)/sizeof(symbols[0]);

//   for (size_t i = 0; i < symsz; ++i) {
//     enum jy_tkn type;
//     size_t line = 1;
//     size_t ofs = 0;
//     char* lexeme = NULL;

//     size_t read = jry_scan(symbols[i].cstr, 2, &type, &line, &ofs, &lexeme);
    
//     ASSERT_EQ(read, 1);
//     ASSERT_EQ(line, 1);
//     ASSERT_EQ(type, symbols[i].type) << "i: " << i;
//     ASSERT_STREQ(lexeme, symbols[i].cstr);

//     jry_free(lexeme);
//   }
// }

// TEST(ScannerTest, ScanKeyword) {
//   struct {
//     char* cstr;
//     size_t size;
//     enum jy_tkn type;
//   } keywords[] = {
//     {"all", 4, TKN_ALL},
//     {"and", 4, TKN_AND},
//     {"any", 4,TKN_ANY},
//     {"false", 6, TKN_FALSE}, 
//     {"true", 5, TKN_TRUE},
//     {"or", 3, TKN_OR},
//     {"not", 4, TKN_NOT},
//     {"input", 6, TKN_INPUT},
//     {"rule", 5, TKN_RULE},
//     {"import", 7, TKN_IMPORT},
//     {"ingress", 8, TKN_INGRESS},
//     {"include", 8, TKN_INCLUDE},
//     {"match", 6, TKN_MATCH},
//     {"target", 7, TKN_TARGET},
//     {"condition", 10, TKN_CONDITION},
//     {"fields", 7, TKN_FIELDS},
//   };

//   size_t kwsz = sizeof(keywords)/sizeof(keywords[0]);

//   for (size_t i = 0; i < kwsz; ++i) { // Check every keyword
//     enum jy_tkn type;
//     size_t line = 1;
//     size_t ofs = 0;
//     char* lexeme = NULL;

//     size_t read = jry_scan(keywords[i].cstr, keywords[i].size, &type, &line, &ofs, &lexeme);
    
//     ASSERT_EQ(read, keywords[i].size - 1) << "i: " << i;
//     ASSERT_EQ(line, 1) << "i: " << i;
//     ASSERT_EQ(type, keywords[i].type) << "i: " << i;
//     ASSERT_STREQ(lexeme, keywords[i].cstr) << "i: " << i;

//     jry_free(lexeme);
//   }
// }

// TEST(ScannerTest, ScanString) {
//     enum jy_tkn type;
//     size_t line = 1;
//     size_t ofs = 0;
//     char* lexeme = NULL;
//     char str[] = "\"hello world\"";
//     size_t len = sizeof(str);

//     size_t read = jry_scan(str, len, &type, &line, &ofs, &lexeme);

//     ASSERT_EQ(type, TKN_STRING);
//     ASSERT_EQ(read, sizeof(str) - 1);
//     ASSERT_STREQ(lexeme, str);
//     ASSERT_EQ(line, 1);

//     jry_free(lexeme);
// }

// TEST(ScannerTest, ScanRegexp) {
//   char str[] = "/Hello world\\//";

//   enum jy_tkn type;
//   size_t line = 1;
//   size_t ofs = 0;
//   size_t len = sizeof(str);
//   char* lexeme = NULL;

//   size_t read = jry_scan(str, len, &type, &line, &ofs, &lexeme);

//   ASSERT_EQ(type, TKN_REGEXP);
//   ASSERT_EQ(read, sizeof(str) - 1);
//   ASSERT_STREQ(lexeme, str);
//   ASSERT_EQ(line, 1);

//   jry_free(lexeme);
// }

// TEST(ScannerTest, ScanIdentifier) {
//   { // legal identifiers
//     std::vector<std::string> ident = {
//       "hello",
//       "_my_name"
//       "identifier123",
//     };

//     for (size_t i = 0; i < ident.size(); ++i) {
//       char str[ident[i].size() + 1] = "\0";
//       enum jy_tkn type;
//       size_t line = 0;
//       size_t ofs = 0;
//       char* lexeme = NULL;
//       size_t len = sizeof(str);
      
//       std::strcpy(str, ident[i].c_str());
      
//       jry_scan(str, len, &type, &line, &ofs, &lexeme);

//       ASSERT_STREQ(lexeme, str);
//       ASSERT_EQ(type, TKN_IDENTIFIER);

//       jry_free(lexeme);
//     }
//   }
// }