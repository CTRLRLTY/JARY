#include <gtest/gtest.h>

extern "C" {
#include "compiler.h"
#include "storage.h"
}

/*TEST(StorageTest, MakeSqlString)*/
/*{*/
/*	const char src[]     = "ingress data {"*/
/*			       "\n"*/
/*			       "       field:"*/
/*			       "\n"*/
/*			       "               yes == string"*/
/*			       "\n"*/
/*			       "               no == string"*/
/*			       "\n"*/
/*			       "}";*/
/**/
/*	const char crt_sql[] = "CREATE TABLE IF NOT EXISTS data ("*/
/*			       "yes TEXT,"*/
/*			       "no TEXT"*/
/*			       ");";*/
/**/
/*	const char ins_sql[] = "INSERT INTO data (yes,no) VALUES (?,?);";*/
/**/
/*	struct jy_asts asts;*/
/*	struct jy_tkns tkns;*/
/*	struct jy_errs errs;*/
/*	struct jy_jay  jay;*/
/**/
/*	memset(&asts, 0, sizeof(asts));*/
/*	memset(&tkns, 0, sizeof(tkns));*/
/*	memset(&errs, 0, sizeof(errs));*/
/*	memset(&jay, 0, sizeof(jay));*/
/**/
/*	jay.mdir = "../modules/";*/
/**/
/*	jry_parse(src, sizeof(src), &asts, &tkns, &errs);*/
/**/
/*	ASSERT_EQ(errs.size, 0);*/
/**/
/*	jry_compile(&asts, &tkns, &jay, &errs);*/
/**/
/*	ASSERT_EQ(errs.size, 0);*/
/**/
/*	uint32_t id;*/
/*	ASSERT_TRUE(jry_find_def(jay.names, "data", &id));*/
/**/
/*	auto  event = jay.names->vals[id].def;*/
/*	char *sql   = NULL;*/
/**/
/*	ASSERT_EQ(jry_sqlstr_crt_event("data", event->capacity, event->keys,*/
/*				       event->types, &sql),*/
/*		  0);*/
/**/
/*	ASSERT_STREQ(crt_sql, sql);*/
/**/
/*	free(sql);*/
/**/
/*	ASSERT_EQ(jry_sqlstr_ins_event("data", event->capacity, event->keys,*/
/*				       &sql),*/
/*		  0);*/
/**/
/*	ASSERT_STREQ(ins_sql, sql);*/
/**/
/*	free(sql);*/
/**/
/*	jry_free_asts(asts);*/
/*	jry_free_tkns(tkns);*/
/*	jry_free_jay(jay);*/
/*	jry_free_errs(errs);*/
/*}*/
