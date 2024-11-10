/*
BSD 3-Clause License

Copyright (c) 2024. Muhammad Raznan. All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <gtest/gtest.h>

extern "C" {
#include "ast.h"
#include "compiler.h"
#include "error.h"
#include "exec.h"
#include "parser.h"
#include "token.h"

#include "jary/defs.h"
#include "jary/memory.h"
#include "jary/types.h"

#include <sqlite3.h>
}

static size_t read_file(const char *path, char **dst)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	uint32_t file_size = ftell(file);
	rewind(file);

	auto   buffer	  = (char *) malloc(file_size + 1);
	size_t bytes_read = fread(buffer, sizeof(char), file_size, file);

	if (bytes_read < file_size) {
		fprintf(stderr, "could not read file \"%s\".\n", path);
		exit(74);
	}

	buffer[bytes_read] = '\0';
	*dst		   = buffer;

	fclose(file);
	return file_size + 1;
}

TEST(ExecTest, MarkModule)
{
	union jy_value key;
	struct jy_str *str;

	struct jy_asts	asts  = { .tkns = NULL };
	struct jy_tkns	tkns  = { .lexemes = NULL };
	struct tkn_errs errs  = { .from = NULL };
	struct jy_jay	jay   = { .codes = NULL };
	struct sc_mem	alloc = { .buf = NULL };
	struct jy_defs *mark  = NULL;
	struct sqlite3 *db    = NULL;
	char	       *src   = NULL;
	size_t		srcsz = read_file(MARK_MODULE_JARY_PATH, &src);

	str = (jy_str *) sc_alloc(&alloc, sizeof(*str) + 6);
	sc_reap(&alloc, src, free);

	str->size = 5;

	strcpy(str->cstr, "hello");

	key.str		  = str;
	const char mdir[] = "../modules/";

	jry_parse(&alloc, &asts, &tkns, &errs, src, srcsz);

	ASSERT_EQ(errs.size, 0);

	jry_compile(&alloc, &jay, &errs, mdir, &asts, &tkns);

	ASSERT_EQ(errs.size, 0);

	{
		uint32_t id;

		ASSERT_TRUE(def_find(jay.names, "mark", &id));

		mark = jay.names->vals[id].module;
	}

	{
		uint32_t	id;
		struct jy_func *ofunc;

		ASSERT_TRUE(def_find(mark, "mark", &id));

		ofunc = mark->vals[id].func;

		// calling mark function -> mark.mark()
		ASSERT_EQ(ofunc->func(NULL, 1, &key, NULL), 0);
	}

	{
		uint32_t	id;
		union jy_value	result;
		struct jy_func *ofunc;

		ASSERT_TRUE(def_find(mark, "count", &id));

		ofunc = mark->vals[id].func;

		// calling count function -> mark.count()
		ASSERT_EQ(ofunc->func(NULL, 1, &key, &result), 0);
		ASSERT_EQ(result.i64, 1);
	}

	{
		uint32_t	id;
		struct jy_func *ofunc;

		ASSERT_TRUE(def_find(mark, "unmark", &id));

		ofunc = mark->vals[id].func;

		// calling count function -> mark.unmark()
		ASSERT_EQ(ofunc->func(NULL, 1, &key, NULL), 0);
	}

	int flag = SQLITE_OPEN_MEMORY | SQLITE_OPEN_PRIVATECACHE
		 | SQLITE_OPEN_READWRITE;
	int err = sqlite3_open_v2("test.db", &db, flag, NULL);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << sqlite3_errmsg(db);

	char *sql = "CREATE TABLE data (yes TEXT);";
	char *msg = NULL;
	err	  = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	sql = "INSERT INTO data (yes) VALUES (\"hello\")";
	err = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	ASSERT_EQ(jry_exec(db, &jay, jay.codes, NULL), 0);

	{
		uint32_t	id;
		union jy_value	result;
		struct jy_func *ofunc;

		ASSERT_TRUE(def_find(mark, "count", &id));

		ofunc = mark->vals[id].func;

		// calling count function -> mark.count()
		ASSERT_EQ(ofunc->func(NULL, 1, &key, &result), 0);
		ASSERT_EQ(result.i64, 1);
	}

	sqlite3_close_v2(db);
	sc_free(&alloc);
}

TEST(ExecTest, Join)
{
	union jy_value key;
	struct jy_str *str;

	struct jy_asts	asts  = { .tkns = NULL };
	struct jy_tkns	tkns  = { .lexemes = NULL };
	struct tkn_errs errs  = { .from = NULL };
	struct jy_jay	jay   = { .codes = NULL };
	struct sc_mem	alloc = { .buf = NULL };
	struct jy_defs *mark  = NULL;
	struct sqlite3 *db    = NULL;
	char	       *src   = NULL;
	size_t		srcsz = read_file(JOIN_JARY_PATH, &src);

	str = (jy_str *) sc_alloc(&alloc, sizeof(*str) + 6);
	sc_reap(&alloc, src, free);

	str->size = 5;

	strcpy(str->cstr, "hello");

	key.str		  = str;
	const char mdir[] = "../modules/";

	jry_parse(&alloc, &asts, &tkns, &errs, src, srcsz);

	ASSERT_EQ(errs.size, 0);

	jry_compile(&alloc, &jay, &errs, mdir, &asts, &tkns);

	ASSERT_EQ(errs.size, 0);

	uint32_t id;

	ASSERT_TRUE(def_find(jay.names, "mark", &id));

	mark = jay.names->vals[id].module;

	int flag = SQLITE_OPEN_MEMORY | SQLITE_OPEN_PRIVATECACHE
		 | SQLITE_OPEN_READWRITE;
	int err = sqlite3_open_v2("test.db", &db, flag, NULL);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << sqlite3_errmsg(db);

	char *sql = "CREATE TABLE data1 (yes TEXT, nein TEXT);";
	char *msg = NULL;
	err	  = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	sql = "CREATE TABLE data2 (no TEXT);";
	msg = NULL;
	err = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	sql = "INSERT INTO data1 (yes, nein) VALUES ('hello', 'goodbye'), "
	      "('hello', 'bye?')";
	err = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	sql = "INSERT INTO data2 (no) VALUES (\"hello\")";
	err = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	ASSERT_EQ(jry_exec(db, &jay, jay.codes, NULL), 0);

	{
		uint32_t	id;
		union jy_value	result;
		struct jy_func *ofunc;

		ASSERT_TRUE(def_find(mark, "count", &id));

		ofunc = mark->vals[id].func;

		// calling count function -> mark.count()
		ASSERT_EQ(ofunc->func(NULL, 1, &key, &result), 0);
		ASSERT_EQ(result.i64, 2);
	}

	sqlite3_close_v2(db);
	sc_free(&alloc);
}

TEST(ExecTest, Within)
{
	union jy_value key;
	struct jy_str *str;

	struct jy_asts	asts  = { .tkns = NULL };
	struct jy_tkns	tkns  = { .lexemes = NULL };
	struct tkn_errs errs  = { .from = NULL };
	struct jy_jay	jay   = { .codes = NULL };
	struct sc_mem	alloc = { .buf = NULL };
	struct jy_defs *mark  = NULL;
	struct sqlite3 *db    = NULL;
	char	       *src   = NULL;
	size_t		srcsz = read_file(WITHIN_JARY_PATH, &src);

	str = (jy_str *) sc_alloc(&alloc, sizeof(*str) + 6);
	sc_reap(&alloc, src, free);

	str->size = 5;

	strcpy(str->cstr, "hello");

	key.str		  = str;
	const char mdir[] = "../modules/";

	jry_parse(&alloc, &asts, &tkns, &errs, src, srcsz);

	ASSERT_EQ(errs.size, 0);

	jry_compile(&alloc, &jay, &errs, mdir, &asts, &tkns);

	ASSERT_EQ(errs.size, 0);

	uint32_t id;

	ASSERT_TRUE(def_find(jay.names, "mark", &id));

	mark = jay.names->vals[id].module;

	int flag = SQLITE_OPEN_MEMORY | SQLITE_OPEN_PRIVATECACHE
		 | SQLITE_OPEN_READWRITE;
	int err = sqlite3_open_v2("test.db", &db, flag, NULL);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << sqlite3_errmsg(db);

	char *sql = "CREATE TABLE data (yes TEXT, __arrival__ "
		    "INTEGER DEFAULT (unixepoch()));";
	char *msg = NULL;
	err	  = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	sql = "INSERT INTO data (yes) VALUES ('hello')";
	err = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	ASSERT_EQ(jry_exec(db, &jay, jay.codes, NULL), 0);

	union jy_value	result;
	struct jy_func *ofunc;

	ASSERT_TRUE(def_find(mark, "count", &id));

	ofunc = mark->vals[id].func;

	// calling count function -> mark.count()
	ASSERT_EQ(ofunc->func(NULL, 1, &key, &result), 0);
	ASSERT_EQ(result.i64, 1);

	sqlite3_close_v2(db);
	sc_free(&alloc);
}

TEST(ExecTest, Between)
{
	union jy_value key;
	struct jy_str *str;

	struct jy_asts	asts  = { .tkns = NULL };
	struct jy_tkns	tkns  = { .lexemes = NULL };
	struct tkn_errs errs  = { .from = NULL };
	struct jy_jay	jay   = { .codes = NULL };
	struct sc_mem	alloc = { .buf = NULL };
	struct jy_defs *mark  = NULL;
	struct sqlite3 *db    = NULL;
	char	       *src   = NULL;
	size_t		srcsz = read_file(BETWEEN_JARY_PATH, &src);

	str = (jy_str *) sc_alloc(&alloc, sizeof(*str) + 6);
	sc_reap(&alloc, src, free);

	str->size = 5;

	strcpy(str->cstr, "hello");

	key.str		  = str;
	const char mdir[] = "../modules/";

	jry_parse(&alloc, &asts, &tkns, &errs, src, srcsz);

	ASSERT_EQ(errs.size, 0);

	jry_compile(&alloc, &jay, &errs, mdir, &asts, &tkns);

	ASSERT_EQ(errs.size, 0);

	uint32_t id;

	ASSERT_TRUE(def_find(jay.names, "mark", &id));

	mark = jay.names->vals[id].module;

	int flag = SQLITE_OPEN_MEMORY | SQLITE_OPEN_PRIVATECACHE
		 | SQLITE_OPEN_READWRITE;
	int err = sqlite3_open_v2("test.db", &db, flag, NULL);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << sqlite3_errmsg(db);

	char *sql = "CREATE TABLE data (age INTEGER);";
	char *msg = NULL;
	err	  = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	sql = "INSERT INTO data (age) VALUES (8), (4), (11);";
	err = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	ASSERT_EQ(jry_exec(db, &jay, jay.codes, NULL), 0);

	union jy_value	result;
	struct jy_func *ofunc;

	ASSERT_TRUE(def_find(mark, "count", &id));

	ofunc = mark->vals[id].func;

	// calling count function -> mark.count()
	ASSERT_EQ(ofunc->func(NULL, 1, &key, &result), 0);
	ASSERT_EQ(result.i64, 1);

	sqlite3_close_v2(db);
	sc_free(&alloc);
}

TEST(ExecTest, ExactEqual)
{
	struct jy_asts	asts  = { .tkns = NULL };
	struct jy_tkns	tkns  = { .lexemes = NULL };
	struct tkn_errs errs  = { .from = NULL };
	struct jy_jay	jay   = { .codes = NULL };
	struct sc_mem	alloc = { .buf = NULL };
	struct sb_mem	bump  = { .buf = NULL };
	struct sqlite3 *db    = NULL;
	char	       *src   = NULL;
	size_t		srcsz = read_file(EXACT_EQUAL_JARY_PATH, &src);

	sc_reap(&alloc, src, free);

	const char mdir[] = "../modules/";

	jry_parse(&alloc, &asts, &tkns, &errs, src, srcsz);

	ASSERT_EQ(errs.size, 0);

	jry_compile(&alloc, &jay, &errs, mdir, &asts, &tkns);

	ASSERT_EQ(errs.size, 0);

	int flag = SQLITE_OPEN_MEMORY | SQLITE_OPEN_PRIVATECACHE
		 | SQLITE_OPEN_READWRITE;

	int err = sqlite3_open_v2("test.db", &db, flag, NULL);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << sqlite3_errmsg(db);

	char *sql = "CREATE TABLE data (name TEXT, age INTEGER);"
		    "INSERT INTO data (name, age) VALUES ('root', 18);";
	char *msg = NULL;
	err	  = sqlite3_exec(db, sql, NULL, NULL, &msg);

	ASSERT_EQ(err, SQLITE_OK) << "msg: " << msg;

	struct jy_state state = { .buf = &alloc, .outm = &bump };

	ASSERT_EQ(jry_exec(db, &jay, jay.codes, &state), 0);

	ASSERT_EQ(state.outsz, 1);
	ASSERT_EQ(state.out[0].i64, 42);

	sqlite3_close_v2(db);
	sc_free(&alloc);
	sb_free(&bump);
}
