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
#include "jary/jary.h"
}

struct simple_cb_data {
	char *msg;
	long  count;
};

static int callback(void *data, const struct jyOutput *output)
{
	auto	    view = (simple_cb_data *) data;
	const char *value;
	long	    count = 0;

	if (jary_output_str(output, 0, &value) != JARY_OK)
		return JARY_INT_CRASH; // crash the runtime

	if (jary_output_long(output, 1, &count) != JARY_OK)
		return JARY_INT_CRASH; // crash the runtime

	view->msg   = strdup(value);
	view->count = count;

	return JARY_OK;
}

TEST(JaryModuleTest, Simple)
{
	const char   expect[] = "must've been the wind";
	const char   rule[]   = "auth_brute_force";
	struct jary *J;
	unsigned int ev;

	ASSERT_EQ(jary_open(&J), JARY_OK);
	ASSERT_EQ(jary_modulepath(J, MODULE_DIR), JARY_OK);

	ASSERT_EQ(jary_compile_file(J, SIMPLE_JARY_PATH, NULL), JARY_OK);

	for (int i = 0; i < 10; ++i) {
		ASSERT_EQ(jary_event(J, "user", &ev), JARY_OK);
		ASSERT_EQ(jary_field_str(J, ev, "name", "root"), JARY_OK);
		ASSERT_EQ(jary_field_str(J, ev, "activity", "failed login"),
			  JARY_OK);
	}

	struct simple_cb_data data = { .msg = NULL };

	ASSERT_EQ(jary_rule_clbk(J, rule, callback, &data), JARY_OK);

	ASSERT_EQ(jary_execute(J), JARY_OK);

	ASSERT_STREQ(data.msg, expect);
	ASSERT_EQ(data.count, 10);

	free(data.msg);

	ASSERT_EQ(jary_close(J), JARY_OK);
}
