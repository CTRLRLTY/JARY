#include <gtest/gtest.h>

extern "C" {
#include "vector.h"
}

TEST(VectorTest, Push) {
    {
        vec_t(int) numbers;

        vecinit(numbers, 10);

        vecpush(numbers, 20);

        ASSERT_EQ(vecsize(numbers), 1);

        ASSERT_EQ(numbers[0], 20);

        vecfree(numbers);
    }
}