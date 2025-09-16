#include "strategy/mean_reversion.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace hft
{
namespace
{
class MeanReversionTest : public ::testing::Test
{
protected:
  StrategyContext ctx{};
  RiskManager risk;
  spsc::Queue<EngineCommand, 1 << 14> cmd_q;
  std::unique_ptr<MeanReversion> strategy;

  MeanReversionTest()
      : ctx(), risk(100, 1'000'000, 10), cmd_q()
  {
    ctx.user_id = 1;
    ctx.tick = 1;
    ctx.next_order_id = 1;
  }

  void SetUp() override
  {
    strategy = std::make_unique<MeanReversion>(ctx, risk, cmd_q, 8, 2.0, 2);
  }

  void TearDown() override
  {
    EngineCommand cmd{};
    while (cmd_q.pop(cmd))
    {
    }
  }
};

TEST_F(MeanReversionTest, PostsQuotesWhenTopAvailable)
{
  TopOfBook top{};
  top.bid_price = 100;
  top.bid_qty = 5;
  top.ask_price = 102;
  top.ask_qty = 5;
  top.ts_ns = now_ns();

  strategy->on_market_data(MarketDataEvent{top});
  strategy->on_timer(now_ns());

  EngineCommand cmd{};
  std::vector<EngineCommand> seen;
  while (cmd_q.pop(cmd))
  {
    seen.push_back(cmd);
  }

  ASSERT_EQ(seen.size(), 2U);
  EXPECT_EQ(seen[0].kind, EngineCommand::Kind::New);
  EXPECT_EQ(seen[0].new_order.side, Side::Buy);
  EXPECT_EQ(seen[1].new_order.side, Side::Sell);
  EXPECT_GT(seen[0].new_order.price, 0);
  EXPECT_GT(seen[1].new_order.price, 0);
}

TEST_F(MeanReversionTest, SkipsQuotesWhenRiskBlocks)
{
  StrategyContext alt_ctx = ctx;
  spsc::Queue<EngineCommand, 1 << 14> alt_queue;
  RiskManager tight_risk(100, 1'000'000, 1);
  MeanReversion conservative(alt_ctx, tight_risk, alt_queue, 4, 1.0, 5);

  TopOfBook top{};
  top.bid_price = 200;
  top.ask_price = 202;
  conservative.on_market_data(MarketDataEvent{top});
  conservative.on_timer(now_ns());

  EngineCommand cmd{};
  EXPECT_FALSE(alt_queue.pop(cmd));
}
} // namespace
} // namespace hft
