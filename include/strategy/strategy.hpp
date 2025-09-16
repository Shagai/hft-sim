#pragma once

#include "market/market_data.hpp"
#include "common/spsc_queue.hpp"
#include "market/matching_engine.hpp"
#include "risk/risk_manager.hpp"
#include "common/logging.hpp"

#include <vector>
#include <optional>

namespace hft
{
// Interface for strategies: consume market data and execs, then produce orders back into engine.
// The goal is to let learners focus on signal logic while the surrounding infrastructure stays
// small.
struct StrategyContext
{
  u64 next_order_id{1}; // per-strategy sequence so orders have unique identifiers
  u64 user_id{1};       // injected into orders for routing/risk tracking
  Price tick{1};        // minimum price increment the instrument trades in
};

class IStrategy
{
public:
  virtual ~IStrategy() = default;
  virtual void on_market_data(const MarketDataEvent &e) = 0;
  virtual void on_exec(const ExecEvent &e) = 0;
  virtual void on_timer(u64 ts_ns) = 0;
};
} // namespace hft
