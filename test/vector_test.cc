#include <gtest/gtest.h>

extern "C" {
#include "vector.h"
}

TEST(VectorTest, PushPopGetFind) {
    Vector vec;

    {
        int v[] = {5, 3, 4};

        int buf;

        ASSERT_EQ(vec_init(&vec, sizeof(int)), VEC_SUCCESS);

        ASSERT_EQ(vec_push(&vec, &v[0], sizeof(v[0])), VEC_SUCCESS);

        EXPECT_EQ(vec.count, 1);

        ASSERT_EQ(vec_get(&vec, 0, &buf, sizeof(buf)), VEC_SUCCESS)
        ;
        EXPECT_EQ(buf, v[0]);

        ASSERT_EQ(vec_push(&vec, &v[1],  sizeof(v[1])), VEC_SUCCESS);

        ASSERT_EQ(vec.count, 2);

        ASSERT_EQ(vec_get(&vec, 1, &buf, sizeof(buf)), VEC_SUCCESS);

        EXPECT_EQ(buf, v[1]);

        EXPECT_EQ(vec_find(&vec, &v[0], sizeof(v[0]), &buf, sizeof(buf)), VEC_FOUND);

        EXPECT_EQ(buf, v[0]);

        EXPECT_EQ(vec_find(&vec, &v[1], sizeof(v[1]), &buf, sizeof(buf)), VEC_FOUND);

        EXPECT_EQ(buf, v[1]);

        EXPECT_EQ(vec_find(&vec, &v[2], sizeof(v[2]), &buf, sizeof(buf)), VEC_NOT_FOUND);

        EXPECT_EQ(buf, v[1]);

        ASSERT_EQ(vec_pop(&vec, &buf, sizeof(buf)), VEC_SUCCESS);

        EXPECT_EQ(buf, v[1]);

        EXPECT_EQ(vec.count, 1);

        EXPECT_EQ(vec_pop(&vec, NULL, 0), VEC_SUCCESS);

        EXPECT_EQ(vec.count, 0);

        EXPECT_EQ(vec_free(&vec), VEC_SUCCESS);
    }
}