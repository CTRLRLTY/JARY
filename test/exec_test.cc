#include <gtest/gtest.h>

extern "C" {
#include "compiler.h"
#include "exec.h"

#include "jary/memory.h"
#include "jary/object.h"
}

TEST(ExecTest, MarkModule)
{
	union jy_value	key;
	struct jy_asts	asts;
	struct jy_tkns	tkns;
	struct jy_errs	errs;
	struct jy_defs	names;
	struct jy_jay	jay;
	struct jy_defs *mark;

	memset(&asts, 0, sizeof(asts));
	memset(&tkns, 0, sizeof(tkns));
	memset(&errs, 0, sizeof(errs));
	memset(&names, 0, sizeof(names));
	memset(&jay, 0, sizeof(jay));

	const char src[]       = "import mark"
				 "\n"
				 "\n"
				 "ingress data {"
				 "\n"
				 "       field:"
				 "\n"
				 "               yes = string"
				 "\n"
				 "}"
				 "\n"
				 "rule bye {"
				 "\n"
				 "       match:"
				 "\n"
				 "                $data.yes = \"hello\""
				 "\n"
				 "       condition:"
				 "\n"
				 "               2 = 2"
				 "\n"
				 "       target:"
				 "\n"
				 "               mark.mark($data.yes)"
				 "\n"
				 "}";

	struct jy_obj_str *str = (jy_obj_str *) malloc(sizeof(*str) + 6);
	str->size	       = 5;
	strcpy(str->cstr, "hello");
	key.str	  = str;

	jay.names = &names;
	jay.mdir  = "../modules/";

	jry_parse(src, sizeof(src), &asts, &tkns, &errs);

	ASSERT_EQ(errs.size, 0);

	jry_compile(&asts, &tkns, &jay, &errs);

	ASSERT_EQ(errs.size, 0);

	{
		uint32_t id;
		long	 ofs;

		ASSERT_TRUE(jry_find_def(&names, "mark", &id));

		ofs  = names.vals[id].ofs;
		mark = (struct jy_defs *) memory_fetch(jay.obj.buf, ofs);
	}

	{
		uint32_t	    id;
		long		    ofs;
		struct jy_obj_func *ofunc;

		ASSERT_TRUE(jry_find_def(mark, "mark", &id));

		ofs   = mark->vals[id].ofs;
		ofunc = (struct jy_obj_func *) memory_fetch(jay.obj.buf, ofs);

		// calling mark function -> mark.mark()
		ASSERT_EQ(ofunc->func(1, &key, NULL), 0);
	}

	{
		uint32_t	    id;
		long		    ofs;
		union jy_value	    result;
		struct jy_obj_func *ofunc;

		ASSERT_TRUE(jry_find_def(mark, "count", &id));

		ofs   = mark->vals[id].ofs;
		ofunc = (struct jy_obj_func *) memory_fetch(jay.obj.buf, ofs);

		// calling count function -> mark.count()
		ASSERT_EQ(ofunc->func(1, &key, &result), 0);
		ASSERT_EQ(result.i64, 1);
	}

	{
		uint32_t	    id;
		long		    ofs;
		struct jy_obj_func *ofunc;

		ASSERT_TRUE(jry_find_def(mark, "unmark", &id));

		ofs   = mark->vals[id].ofs;
		ofunc = (struct jy_obj_func *) memory_fetch(jay.obj.buf, ofs);

		// calling count function -> mark.unmark()
		ASSERT_EQ(ofunc->func(1, &key, NULL), 0);
	}

	// set event data.yes = "hello"
	jry_set_event("data", "yes", key, jay.obj.buf, &names);

	jry_exec(jay.vals, jay.types, jay.obj.buf, jay.codes, jay.codesz);

	{
		uint32_t	    id;
		long		    ofs;
		union jy_value	    result;
		struct jy_obj_func *ofunc;

		ASSERT_TRUE(jry_find_def(mark, "count", &id));

		ofs   = mark->vals[id].ofs;
		ofunc = (struct jy_obj_func *) memory_fetch(jay.obj.buf, ofs);

		// calling count function -> mark.count()
		ASSERT_EQ(ofunc->func(1, &key, &result), 0);
		ASSERT_EQ(result.i64, 1);
	}

	jry_free_asts(asts);
	jry_free_tkns(tkns);
	jry_free_jay(jay);
}

TEST(ExecTest, StringConcat)
{
	const char src[] =
		"import mark"
		"\n"
		"rule bye {"
		"       condition:"
		"\n"
		"               \"hello \" + \"world\" = \"hello world\""
		"\n"
		"       target:"
		"\n"
		"               mark.mark(\"test\")"
		"\n"
		"}";

	struct jy_asts asts;
	struct jy_tkns tkns;
	struct jy_errs errs;
	struct jy_defs names;
	struct jy_jay  jay;

	memset(&asts, 0, sizeof(asts));
	memset(&tkns, 0, sizeof(tkns));
	memset(&errs, 0, sizeof(errs));
	memset(&names, 0, sizeof(names));
	memset(&jay, 0, sizeof(jay));

	jay.names = &names;
	jay.mdir  = "../modules/";

	jry_parse(src, sizeof(src), &asts, &tkns, &errs);

	ASSERT_EQ(errs.size, 0);

	jry_compile(&asts, &tkns, &jay, &errs);

	ASSERT_EQ(errs.size, 0) << errs.msgs[0];

	struct jy_defs *mark;

	{
		uint32_t id;
		long	 ofs;

		ASSERT_TRUE(jry_find_def(&names, "mark", &id));

		ofs  = names.vals[id].ofs;
		mark = (struct jy_defs *) memory_fetch(jay.obj.buf, ofs);
	}

	jry_exec(jay.vals, jay.types, jay.obj.buf, jay.codes, jay.codesz);

	{
		uint32_t	    id;
		long		    ofs;
		union jy_value	    key;
		union jy_value	    result;
		struct jy_obj_func *ofunc;
		struct jy_obj_str  *str =
			(jy_obj_str *) malloc(sizeof(*str) + 5);

		str->size = 4;
		strcpy(str->cstr, "test");

		key.str = str;

		ASSERT_TRUE(jry_find_def(mark, "count", &id));

		ofs   = mark->vals[id].ofs;
		ofunc = (struct jy_obj_func *) memory_fetch(jay.obj.buf, ofs);

		// calling count function -> mark.count()
		ASSERT_EQ(ofunc->func(1, &key, &result), 0);
		ASSERT_EQ(result.i64, 1);
	}

	jry_free_asts(asts);
	jry_free_tkns(tkns);
	jry_free_jay(jay);
}
