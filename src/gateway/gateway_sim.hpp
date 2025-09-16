#pragma once
#include <thread>
#include <atomic>
#include "market/matching_engine.hpp"
#include "market/simulator.hpp"
#include "common/spsc_queue.hpp"
#include "common/logging.hpp"

namespace hft
{
// EngineThread wraps the order book + matching engine + simulator in one loop.
// It reads commands from a single SPSC queue and emits execs + market data to their queues.
// Think of it as the "exchange side" counterpart to a strategy: you can plug in different
// strategies without touching this class.
class EngineThread
{
  OrderBook book_;                                           // shared order book instance
  spsc::Queue<EngineCommand, 1 << 14> &cmd_in_;              // strategy -> engine commands
  spsc::Queue<ExecEvent, 1 << 14> &exec_out_;                // exec reports -> strategy
  spsc::Queue<MarketDataEvent, 1 << 14> &md_out_;            // market data -> strategy
  Simulator sim_;                                            // generates artificial street flow
  std::atomic<bool> running_{false};                         // controls lifecycle of the thread
  std::thread thread_;                                       // actual engine worker thread

public:
  EngineThread(spsc::Queue<EngineCommand, 1 << 14> &cmd_in,
               spsc::Queue<ExecEvent, 1 << 14> &exec_out,
               spsc::Queue<MarketDataEvent, 1 << 14> &md_out, StreetFlowConfig cfg = {})
      : cmd_in_(cmd_in), exec_out_(exec_out), md_out_(md_out), sim_(cfg)
  {
  }

  void start()
  {
    // Launch the engine thread. The lambda captures `this` so run() operates on the same object.
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { this->run(); });
  }

  void stop()
  {
    // Signal the loop to exit and join the worker before returning to caller.
    running_.store(false, std::memory_order_release);
    if (thread_.joinable())
      thread_.join();
  }

  TopOfBook top_snapshot() const
  {
    // Expose best prices to the simulator (runs on same thread, so no need for locks).
    return book_.top();
  }

  // Used by simulator, runs in the same thread:
  void inject_new(const NewOrder &n)
  {
    // We instantiate a short-lived MatchingEngine view to reuse its command handling helpers.
    MatchingEngine me(book_, exec_out_, md_out_);
    me.on_command(EngineCommand{EngineCommand::Kind::New, n, {}});
  }

  void inject_cancel(const CancelOrder &c)
  {
    // Same story for cancels; by reusing MatchingEngine logic we ensure consistent execs/prints.
    MatchingEngine me(book_, exec_out_, md_out_);
    me.on_command(EngineCommand{EngineCommand::Kind::Cancel, {}, c});
  }

private:
  void run()
  {
    MatchingEngine me(book_, exec_out_, md_out_);
    // Seed book so strategies receive a top-of-book early.
    sim_.seed_book(book_);
    md_out_.push(MarketDataEvent{book_.top()});

    // Loop
    while (running_.load(std::memory_order_acquire))
    {
      // 1) Drain strategy commands
      EngineCommand cmd;
      int drained = 0;
      while (cmd_in_.pop(cmd))
      {
        me.on_command(cmd);
        ++drained;
        if (drained > 256)
          break; // avoid starving simulator
      }

      // 2) Simulate a bit of street flow
      sim_.step(*this);

      // 3) Small pause to avoid burning 100% CPU in sample
      // In a real engine you'd busy-wait or use timerfd. Here we sleep a micro-burst.
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
};
} // namespace hft
