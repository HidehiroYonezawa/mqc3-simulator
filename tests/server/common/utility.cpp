#include "server/common/utility.h"

#include <fmt/core.h>
#include <gtest/gtest.h>

TEST(Clock, Now) {
    auto t = Timestamp::Now();

    std::cout << t.ToString() << std::endl;
    EXPECT_TRUE(t.IsValid());
    EXPECT_TRUE(t.IsInitialized());
}
TEST(Clock, Compare) {
    auto t1 = Timestamp::Now();
    auto t2 = Timestamp::Now();

    EXPECT_FALSE(t1 == t2);
    EXPECT_TRUE(t1 != t2);
    EXPECT_TRUE(t1 < t2);
    EXPECT_TRUE(t1 <= t2);
    EXPECT_FALSE(t1 > t2);
    EXPECT_FALSE(t1 >= t2);
}
TEST(Duration, Initialize) {
    auto t1 = Timestamp::Now();
    auto t2 = Timestamp::Now();
    auto d = t2 - t1;

    std::cout << d.ToString() << std::endl;
    EXPECT_TRUE(d.IsInitialized());
    EXPECT_TRUE(d.IsValid());
}
