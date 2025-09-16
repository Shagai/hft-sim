#include "market/order_book.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace hft
{
namespace
{
TEST(OrderBookTest, TopReflectsBestBidAsk)
{
  OrderBook book;
  EXPECT_TRUE(book.empty());

  NewOrder buy{1, 42, Side::Buy, 100, 5, TIF::Day, now_ns()};
  NewOrder sell{2, 42, Side::Sell, 105, 3, TIF::Day, now_ns()};
  book.add_passive(buy);
  book.add_passive(sell);

  TopOfBook top = book.top();
  EXPECT_EQ(top.bid_price, 100);
  EXPECT_EQ(top.bid_qty, 5);
  EXPECT_EQ(top.ask_price, 105);
  EXPECT_EQ(top.ask_qty, 3);
  EXPECT_FALSE(book.empty());
}

TEST(OrderBookTest, CancelRemovesOrder)
{
  OrderBook book;
  book.add_passive(NewOrder{10, 7, Side::Buy, 99, 8, TIF::Day, now_ns()});

  Qty canceled = book.cancel(10);
  EXPECT_EQ(canceled, 8);
  EXPECT_TRUE(book.empty());

  EXPECT_EQ(book.cancel(999), 0); // unknown id
}

TEST(OrderBookTest, MatchConsumesLiquidity)
{
  OrderBook book;
  book.add_passive(NewOrder{200, 8, Side::Sell, 105, 4, TIF::Day, now_ns()});

  std::vector<Qty> fills;
  Qty remaining = book.match(NewOrder{201, 8, Side::Buy, 105, 3, TIF::Day, now_ns()},
                             [&](Price px, Qty qty, const Order &resting)
                             {
                               (void)resting;
                               EXPECT_EQ(px, 105);
                               fills.push_back(qty);
                             });

  ASSERT_EQ(fills.size(), 1U);
  EXPECT_EQ(fills.front(), 3);
  EXPECT_EQ(remaining, 0);

  TopOfBook top = book.top();
  EXPECT_EQ(top.ask_price, 105);
  EXPECT_EQ(top.ask_qty, 1);
}
} // namespace
} // namespace hft
