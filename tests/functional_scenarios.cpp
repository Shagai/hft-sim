#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
#include "gateway/gateway_sim.hpp"
#include "strategy/mean_reversion.hpp"
#include "risk/risk_manager.hpp"
#include "common/logging.hpp"

using namespace hft;

// Functional tests using only the simulator. No external deps.
int main()
{
    spsc::Queue<EngineCommand, 1 << 14> cmd_q;
    spsc::Queue<ExecEvent,     1 << 14> exec_q;
    spsc::Queue<MarketDataEvent, 1 << 14> md_q;

    EngineThread engine(cmd_q, exec_q, md_q, StreetFlowConfig{});
    engine.start();

    StrategyContext ctx;
    ctx.user_id = 42;
    ctx.next_order_id = 10;
    ctx.tick = 1;

    RiskManager risk(200, 5'000'000, 50);
    MeanReversion strat(ctx, risk, cmd_q, 64, 2.0, 5);

    std::atomic<bool> running{true};
    std::atomic<int> trade_count{0};

    // Exec consumer counts trades
    std::thread exec_thread([&] {
        ExecEvent e;
        while (running.load(std::memory_order_acquire))
        {
            while (exec_q.pop(e))
            {
                strat.on_exec(e);
                if (e.type == ExecType::Trade) trade_count.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // Market data consumer
    std::thread md_thread([&] {
        MarketDataEvent ev;
        while (running.load(std::memory_order_acquire))
        {
            while (md_q.pop(ev)) strat.on_market_data(ev);
            strat.on_timer(now_ns());
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });

    // Run scenario
    std::this_thread::sleep_for(std::chrono::seconds(4));
    running.store(false);

    exec_thread.join();
    md_thread.join();
    engine.stop();

    // Assertions
    assert(trade_count.load() > 0 && "Strategy should trade at least once");
    HFT_INFO("Functional test passed with %d trades.", trade_count.load());
    return 0;
}
