#pragma once
#include <vector>
#include <optional>
#include "market_data.hpp"
#include "common/spsc_queue.hpp"
#include "market/matching_engine.hpp"
#include "risk/risk_manager.hpp"
#include "common/logging.hpp"

namespace hft
{
    // Interface for strategies: consume market data and execs, produce orders.
    struct StrategyContext
    {
        u64 next_order_id{1};
        u64 user_id{1};
        Price tick{1};
    };

    class IStrategy
    {
    public:
        virtual ~IStrategy() = default;
        virtual void on_market_data(const MarketDataEvent& e) = 0;
        virtual void on_exec(const ExecEvent& e) = 0;
        virtual void on_timer(u64 ts_ns) = 0;
    };
} // namespace hft
