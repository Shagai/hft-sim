#pragma once
#include <atomic>
#include "market/order.hpp"

namespace hft
{
    // Basic per-strategy risk limits: max position, max notional, and per-order size.
    class RiskManager
    {
        std::atomic<i64> position_{0}; // signed lots
        std::atomic<i64> notional_{0}; // sum(|price*qty|); simplistic
        i64 max_position_;
        i64 max_notional_;
        Qty max_order_qty_;

    public:
        RiskManager(i64 max_position, i64 max_notional, Qty max_order_qty)
            : max_position_(max_position), max_notional_(max_notional), max_order_qty_(max_order_qty) {}

        bool can_quote(Qty q) const noexcept
        {
            return q <= max_order_qty_ && std::abs(position_.load(std::memory_order_relaxed)) < max_position_;
        }

        void on_exec(const ExecEvent& e) noexcept
        {
            if (e.type != ExecType::Trade) return;
            const i64 dir = (e.filled > 0) ? 1 : 0; // qty is always > 0; direction from context below
            (void)dir; // not used in this simplified example
            // Here we only track notional. A real model would track per-symbol limits and side.
            notional_.fetch_add(static_cast<i64>(std::llabs(static_cast<long long>(e.price) * e.filled)),
                                std::memory_order_relaxed);
            // You can update position by listening to side; omitted for brevity.
        }
    };
} // namespace hft
