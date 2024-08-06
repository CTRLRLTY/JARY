#include <gtest/gtest.h>

extern "C" {
#include "vector.h"
}

TEST(VectorTest, Push) {
    {
        jary_vec_t(int) numbers;

        jary_vec_init(numbers, 10);

        jary_vec_push(numbers, 20);

        ASSERT_EQ(jary_vec_size(numbers), 1);

        ASSERT_EQ(numbers[0], 20);

        jary_vec_free(numbers);
    }
}