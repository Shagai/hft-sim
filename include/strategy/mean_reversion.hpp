#pragma once

#include "strategy.hpp"

namespace hft
{
// A tiny mean-reversion maker: maintain a rolling mean of mid-price.
// If mid deviates by N ticks, quote both sides around mid, lean into deviation.
// This file intentionally contains the full implementation so readers can inspect the whole flow
// without jumping between declaration/definition files.
class MeanReversion final : public IStrategy
{
  StrategyContext &ctx_;                     // shared counters (order ids, tick size, etc.)
  RiskManager &risk_;                        // guard rails to avoid runaway quoting
  spsc::Queue<EngineCommand, 1 << 14> &out_; // queue into the engine thread
  std::vector<Price> window_;                // rolling window storing historical mids
  std::size_t wlen_;                         // cached window length to avoid repeated size()
  double dev_ticks_;                         // deviation threshold expressed in ticks
  Qty quote_qty_;                            // quantity per quote
  Price last_bid_{0}, last_ask_{0};          // last prices we quoted (for potential cancels)
  TopOfBook last_top_{};                     // most recent market snapshot seen

public:
  MeanReversion(StrategyContext &ctx, RiskManager &risk, spsc::Queue<EngineCommand, 1 << 14> &out,
                std::size_t window_len = 64, double dev_ticks = 2.0, Qty quote_qty = 1)
      : ctx_(ctx), risk_(risk), out_(out), window_(window_len, 0), wlen_(window_len),
        dev_ticks_(dev_ticks), quote_qty_(quote_qty)
  {
  }

  void on_market_data(const MarketDataEvent &e) override
  {
    if (std::holds_alternative<TopOfBook>(e))
    {
      last_top_ = std::get<TopOfBook>(e);
      if (last_top_.bid_price > 0 && last_top_.ask_price > 0)
      {
        const Price mid = (last_top_.bid_price + last_top_.ask_price) / 2;
        // Update the rolling window with the fresh midpoint.
        rotate_and_push(mid);
      }
    }
  }

  void on_exec(const ExecEvent &e) override
  {
    // Feed trade information into the risk manager so subsequent can_quote checks stay current.
    risk_.on_exec(e);
  }

  void on_timer(u64 ts_ns) override
  {
    // If no book yet, do nothing.
    if (last_top_.bid_price == 0 || last_top_.ask_price == 0)
      return;

    const Price mid = (last_top_.bid_price + last_top_.ask_price) / 2;
    const double mean = rolling_mean();
    const double dev = static_cast<double>(mid - static_cast<Price>(mean));
    const Price tick = ctx_.tick;
    const Price edge = static_cast<Price>(dev_ticks_) * tick;

    // Cancel previous quotes if top moved away.
    cancel_if_stale(ts_ns);

    // Basic risk checks before quoting
    if (!risk_.can_quote(quote_qty_))
      return;

    // Lean toward mean: if mid > mean by N ticks, skew quotes to sell more aggressively.
    Price bid_quote = mid - edge;
    Price ask_quote = mid + edge;

    send_new(Side::Buy, bid_quote, quote_qty_, ts_ns);
    send_new(Side::Sell, ask_quote, quote_qty_, ts_ns);

    last_bid_ = bid_quote;
    last_ask_ = ask_quote;
  }

private:
  void rotate_and_push(Price x)
  {
    // O(1) ring buffer rolling mean without recomputing sum each time.
    static_assert(sizeof(std::size_t) == sizeof(u64));
    static u64 idx = 0;
    // Overwrite oldest entry and advance index. This keeps the window at wlen_ samples.
    window_[idx % wlen_] = x;
    ++idx;
  }

  double rolling_mean() const
  {
    // Simple mean to keep the example approachable. Consider incremental mean for efficiency.
    long double sum = 0;
    for (Price p : window_)
      sum += p;
    return static_cast<double>(sum / static_cast<long double>(wlen_));
  }

  void send_new(Side s, Price px, Qty q, u64 ts_ns)
  {
    // Fill out EngineCommand payload and push to engine queue.
    EngineCommand cmd{};
    cmd.kind = EngineCommand::Kind::New;
    cmd.new_order = NewOrder{ctx_.next_order_id++, ctx_.user_id, s, px, q, TIF::Day, ts_ns};
    out_.push(cmd);
  }

  void send_cancel(u64 order_id, u64 ts_ns)
  {
    // Helper for future exercises: demonstrate how to construct cancel commands.
    EngineCommand cmd{};
    cmd.kind = EngineCommand::Kind::Cancel;
    cmd.cancel = CancelOrder{order_id, ctx_.user_id, ts_ns};
    out_.push(cmd);
  }

  void cancel_if_stale(u64 ts_ns)
  {
    // This example does not store our working order ids per price. In production do that.
    // For now this is a no-op. Left as a placeholder for user extensions.
    (void)ts_ns;
  }
};
} // namespace hft
