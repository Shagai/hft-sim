#include <cstdio>
#include <thread>
#include "gateway/gateway_sim.hpp"
#include "common/spsc_queue.hpp"
#include "common/logging.hpp"

using namespace hft;

// This executable runs only the engine + simulator without any strategy.
// Handy for profiling the matching engine and simulator in isolation or for unit tests.
int main()
{
  spsc::Queue<EngineCommand, 1 << 14> cmd_q;
  spsc::Queue<ExecEvent, 1 << 14> exec_q;
  spsc::Queue<MarketDataEvent, 1 << 14> md_q;

  EngineThread engine(cmd_q, exec_q, md_q, StreetFlowConfig{});
  engine.start();

  // Let the simulator churn for a few seconds.
  std::this_thread::sleep_for(std::chrono::seconds(3));

  engine.stop();
  HFT_INFO("Simulator finished.");
  return 0;
}
