#include <thread>
#include <atomic>
#include <cstdio>
#include "common/spsc_queue.hpp"
#include "market/market_data.hpp"
#include "market/matching_engine.hpp"
#include "gateway/gateway_sim.hpp"
#include "strategy/mean_reversion.hpp"
#include "risk/risk_manager.hpp"
#include "common/logging.hpp"

using namespace hft;

int main()
{
    // Queues: strategy -> engine, engine -> strategy (execs), engine -> strategy (market data)
    spsc::Queue<EngineCommand, 1 << 14> cmd_q;
    spsc::Queue<ExecEvent,     1 << 14> exec_q;
    spsc::Queue<MarketDataEvent, 1 << 14> md_q;

    // Start engine + simulator
    EngineThread engine(cmd_q, exec_q, md_q, StreetFlowConfig{});
    engine.start();

    // Strategy components
    StrategyContext ctx;
    ctx.user_id = 1;
    ctx.next_order_id = 1;
    ctx.tick = 1;

    RiskManager risk(/*max_position*/100, /*max_notional*/1'000'000, /*max_order_qty*/10);
    MeanReversion strat(ctx, risk, cmd_q, /*window_len*/64, /*dev_ticks*/2.0, /*quote_qty*/2);

    std::atomic<bool> running{true};

    // Thread to consume execs
    std::thread exec_thread([&] {
        ExecEvent e;
        while (running.load(std::memory_order_acquire))
        {
            while (exec_q.pop(e)) strat.on_exec(e);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // Thread to consume market data and drive timer
    std::thread md_thread([&] {
        MarketDataEvent ev;
        while (running.load(std::memory_order_acquire))
        {
            while (md_q.pop(ev)) strat.on_market_data(ev);
            strat.on_timer(now_ns());
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });

    // Run for a short demo interval
    std::this_thread::sleep_for(std::chrono::seconds(5));
    running.store(false, std::memory_order_release);

    exec_thread.join();
    md_thread.join();
    engine.stop();

    HFT_INFO("Done.");
    return 0;
}
