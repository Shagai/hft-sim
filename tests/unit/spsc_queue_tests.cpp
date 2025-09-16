#include "common/spsc_queue.hpp"

#include <gtest/gtest.h>

namespace hft::spsc
{
namespace
{
TEST(SpscQueueTest, PushPopRoundTrip)
{
  Queue<int, 8> q;

  EXPECT_TRUE(q.empty());
  EXPECT_TRUE(q.push(1));
  EXPECT_TRUE(q.push(2));
  EXPECT_TRUE(q.push(3));
  EXPECT_EQ(q.size(), 3U);

  int value = 0;
  ASSERT_TRUE(q.pop(value));
  EXPECT_EQ(value, 1);
  ASSERT_TRUE(q.pop(value));
  EXPECT_EQ(value, 2);
  ASSERT_TRUE(q.pop(value));
  EXPECT_EQ(value, 3);
  EXPECT_TRUE(q.empty());
}

TEST(SpscQueueTest, RejectsPushWhenFull)
{
  Queue<int, 2> q;

  EXPECT_TRUE(q.push(10));
  EXPECT_TRUE(q.push(20));
  EXPECT_FALSE(q.push(30));
  EXPECT_EQ(q.size(), 2U);

  int value = 0;
  ASSERT_TRUE(q.pop(value));
  EXPECT_EQ(value, 10);
  ASSERT_TRUE(q.pop(value));
  EXPECT_EQ(value, 20);
  EXPECT_FALSE(q.pop(value));
  EXPECT_TRUE(q.empty());
}
} // namespace
} // namespace hft::spsc
