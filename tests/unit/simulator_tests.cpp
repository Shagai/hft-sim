#include "market/order_book.hpp"
#include "market/simulator.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace hft
{
namespace
{
TEST(SimulatorTest, SeedBookInitializesDepth)
{
  OrderBook book;
  StreetFlowConfig cfg{};
  cfg.mid = 10'000;
  cfg.tick = 5;
  cfg.max_depth_levels = 3;

  Simulator sim(cfg);
  sim.seed_book(book);

  TopOfBook top = book.top();
  EXPECT_EQ(top.bid_price, cfg.mid - cfg.tick);
  EXPECT_EQ(top.ask_price, cfg.mid + cfg.tick);
  EXPECT_EQ(top.bid_qty, 10);
  EXPECT_EQ(top.ask_qty, 10);
}

struct RecordingEngine
{
  TopOfBook snapshot{};
  std::vector<NewOrder> seen;

  TopOfBook top_snapshot()
  {
    return snapshot;
  }

  void inject_new(const NewOrder &n)
  {
    seen.push_back(n);
  }
};

TEST(SimulatorTest, StepTightensSpreadWhenConfigured)
{
  StreetFlowConfig cfg{};
  cfg.mid = 100;
  cfg.tick = 1;
  cfg.spread_prob = 1.0;  // always tighten
  cfg.move_prob = 0.0;    // never send market order
  cfg.seed = 1234;

  Simulator sim(cfg);

  RecordingEngine engine;
  engine.snapshot.bid_price = 100;
  engine.snapshot.ask_price = 104;

  sim.step(engine);

  ASSERT_EQ(engine.seen.size(), 2U);
  EXPECT_EQ(engine.seen[0].side, Side::Buy);
  EXPECT_EQ(engine.seen[0].price, 101);
  EXPECT_EQ(engine.seen[1].side, Side::Sell);
  EXPECT_EQ(engine.seen[1].price, 103);
}
} // namespace
} // namespace hft
