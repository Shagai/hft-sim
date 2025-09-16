#include "market/matching_engine.hpp"

#include <gtest/gtest.h>

#include <variant>

namespace hft
{
namespace
{
TEST(MatchingEngineTest, PublishesAckForPassiveOrder)
{
  OrderBook book;
  spsc::Queue<ExecEvent, 1 << 14> exec_q;
  spsc::Queue<MarketDataEvent, 1 << 14> md_q;
  MatchingEngine engine(book, exec_q, md_q);

  EngineCommand cmd{};
  cmd.kind = EngineCommand::Kind::New;
  cmd.new_order = NewOrder{1, 1, Side::Buy, 100, 5, TIF::Day, now_ns()};

  engine.on_command(cmd);

  ExecEvent exec{};
  ASSERT_TRUE(exec_q.pop(exec));
  EXPECT_EQ(exec.type, ExecType::Ack);
  EXPECT_EQ(exec.order_id, 1);
  EXPECT_EQ(exec.leaves, 5);

  MarketDataEvent ev;
  ASSERT_TRUE(md_q.pop(ev));
  ASSERT_TRUE(std::holds_alternative<TopOfBook>(ev));
  const TopOfBook &top = std::get<TopOfBook>(ev);
  EXPECT_EQ(top.bid_price, 100);
  EXPECT_EQ(top.bid_qty, 5);
}

TEST(MatchingEngineTest, MatchesAgainstRestingLiquidity)
{
  OrderBook book;
  book.add_passive(NewOrder{50, 2, Side::Sell, 101, 4, TIF::Day, now_ns()});

  spsc::Queue<ExecEvent, 1 << 14> exec_q;
  spsc::Queue<MarketDataEvent, 1 << 14> md_q;
  MatchingEngine engine(book, exec_q, md_q);

  EngineCommand aggressive{};
  aggressive.kind = EngineCommand::Kind::New;
  aggressive.new_order = NewOrder{60, 3, Side::Buy, 101, 3, TIF::Day, now_ns()};

  engine.on_command(aggressive);

  ExecEvent trade{};
  ASSERT_TRUE(exec_q.pop(trade));
  EXPECT_EQ(trade.type, ExecType::Trade);
  EXPECT_EQ(trade.filled, 3);
  EXPECT_EQ(trade.price, 101);
  EXPECT_EQ(trade.order_id, 60);

  MarketDataEvent ev;
  ASSERT_TRUE(md_q.pop(ev));
  ASSERT_TRUE(std::holds_alternative<TradePrint>(ev));
  const TradePrint &print = std::get<TradePrint>(ev);
  EXPECT_EQ(print.price, 101);
  EXPECT_EQ(print.qty, 3);

  ASSERT_TRUE(md_q.pop(ev));
  ASSERT_TRUE(std::holds_alternative<TopOfBook>(ev));
  const TopOfBook &top = std::get<TopOfBook>(ev);
  EXPECT_EQ(top.ask_price, 101);
  EXPECT_EQ(top.ask_qty, 1);
}
} // namespace
} // namespace hft
