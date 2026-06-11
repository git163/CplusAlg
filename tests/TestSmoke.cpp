// tests/TestSmoke.cpp — 冒烟测试，用于确认 GTest 已正确接入。
// 有了第一个真实测试后，可删除此文件。

#include <gtest/gtest.h>

TEST(SmokeTest, TrivialAssertion) {
    EXPECT_EQ(1 + 1, 2);
}
