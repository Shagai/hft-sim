#include "gateway/gateway_sim.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <variant>

namespace hft
{
namespace
{
TEST(EngineThreadTest, PublishesInitialTopOfBook)
{
  spsc::Queue<EngineCommand, 1 << 14> cmd_q;
  spsc::Queue<ExecEvent, 1 << 14> exec_q;
  spsc::Queue<MarketDataEvent, 1 << 14> md_q;

  StreetFlowConfig cfg{};
  cfg.max_depth_levels = 1;
  EngineThread engine(cmd_q, exec_q, md_q, cfg);
  engine.start();

  MarketDataEvent ev;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
  bool received_top = false;
  while (std::chrono::steady_clock::now() < deadline)
  {
    if (md_q.pop(ev))
    {
      if (std::holds_alternative<TopOfBook>(ev))
      {
        const TopOfBook &top = std::get<TopOfBook>(ev);
        EXPECT_GT(top.bid_qty + top.ask_qty, 0);
        received_top = true;
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  engine.stop();
  EXPECT_TRUE(received_top);
}
} // namespace
} // namespace hft
