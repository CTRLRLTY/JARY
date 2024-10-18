
#include <gtest/gtest.h>

extern "C" {
#include "compiler.h"
#include "exec.h"

#include "jary/memory.h"

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
	union jy_value	   key;
	struct jy_obj_str *str;

	struct jy_asts	asts  = { .tkns = NULL };
	struct jy_tkns	tkns  = { .lexemes = NULL };
	struct jy_errs	errs  = { .from = NULL };
	struct jy_jay	jay   = { .codes = NULL };
	struct sc_mem	alloc = { .buf = NULL };
	struct jy_defs *mark  = NULL;
	struct sqlite3 *db    = NULL;
	char	       *src   = NULL;
	size_t		srcsz = read_file(MARK_MODULE_JARY_PATH, &src);

	str = (jy_obj_str *) sc_alloc(&alloc, sizeof(*str) + 6);
	sc_reap(&alloc, src, free);

	str->size = 5;

	strcpy(str->cstr, "hello");

	key.str	 = str;
	jay.mdir = "../modules/";

	jry_parse(&alloc, &asts, &tkns, &errs, src, srcsz);

	ASSERT_EQ(errs.size, 0);

	jry_compile(&alloc, &jay, &errs, &asts, &tkns);

	ASSERT_EQ(errs.size, 0);

	{
		uint32_t id;

		ASSERT_TRUE(def_find(jay.names, "mark", &id));

		mark = jay.names->vals[id].module;
	}

	{
		uint32_t	    id;
		struct jy_obj_func *ofunc;

		ASSERT_TRUE(def_find(mark, "mark", &id));

		ofunc = mark->vals[id].func;

		// calling mark function -> mark.mark()
		ASSERT_EQ(ofunc->func(1, &key, NULL), 0);
	}

	{
		uint32_t	    id;
		union jy_value	    result;
		struct jy_obj_func *ofunc;

		ASSERT_TRUE(def_find(mark, "count", &id));

		ofunc = mark->vals[id].func;

		// calling count function -> mark.count()
		ASSERT_EQ(ofunc->func(1, &key, &result), 0);
		ASSERT_EQ(result.i64, 1);
	}

	{
		uint32_t	    id;
		struct jy_obj_func *ofunc;

		ASSERT_TRUE(def_find(mark, "unmark", &id));

		ofunc = mark->vals[id].func;

		// calling count function -> mark.unmark()
		ASSERT_EQ(ofunc->func(1, &key, NULL), 0);
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

	ASSERT_EQ(jry_exec(db, NULL, &jay), 0);

	{
		uint32_t	    id;
		union jy_value	    result;
		struct jy_obj_func *ofunc;

		ASSERT_TRUE(def_find(mark, "count", &id));

		ofunc = mark->vals[id].func;

		// calling count function -> mark.count()
		ASSERT_EQ(ofunc->func(1, &key, &result), 0);
		ASSERT_EQ(result.i64, 1);
	}

	sqlite3_close_v2(db);
	sc_free(&alloc);
}
