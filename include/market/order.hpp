#pragma once
#include <cstdint>
#include <unordered_map>
#include <map>
#include <deque>
#include "common/types.hpp"

namespace hft
{
// Thin structs that carry only what the engine needs. Think of them as the wire format between
// strategy, matching engine, and simulator. Keeping them POD makes copying cheap and predictable.
struct Order
{
  u64 order_id; // globally unique identifier from the submitting participant
  u64 user_id;  // identifies which strategy/user owns the order (used for routing risk/execs)
  Side side;    // buy or sell
  Price price;  // limit price expressed in ticks
  Qty qty;      // remaining quantity (aggressive orders will shrink this)
  u64 ts_ns;    // submission timestamp for FIFO priority
};

enum class TIF : u8
{
  Day = 0,
  IOC = 1,
  FOK = 2
};

// Command payload used by strategies to submit new orders to the matching engine.
struct NewOrder
{
  u64 order_id;
  u64 user_id;
  Side side;
  Price price;
  Qty qty;
  TIF tif{TIF::Day};
  u64 ts_ns{0};
};

// Cancel request containing just enough information to target a resting order.
struct CancelOrder
{
  u64 order_id;
  u64 user_id;
  u64 ts_ns{0};
};

// Minimal execution report types from engine to strategy. The enum keeps payload size tiny while
// covering the typical lifecycle states you'll see on real exchanges.
enum class ExecType : u8
{
  Ack,
  Trade,
  CancelAck,
  Reject,
  DoneForDay
};

// Execution reports the engine publishes back to the strategy. Only the fields relevant for the
// selected ExecType are populated to avoid wasting bandwidth in the SPSC queue.
struct ExecEvent
{
  ExecType type{ExecType::Ack};
  u64 order_id{0};
  u64 user_id{0};
  Qty filled{0};  // for Trade
  Price price{0}; // for Trade
  Qty leaves{0};  // remaining
  sv reason{};    // for Reject
  u64 ts_ns{0};
};

// Market data events pushed to strategies. Keep it tiny for cache efficiency—the queues often live
// in shared memory between CPU cores so smaller payload means fewer cache misses.
struct TopOfBook
{
  Price bid_price{0};
  Qty bid_qty{0};
  Price ask_price{0};
  Qty ask_qty{0};
  u64 ts_ns{0};
};

// Trade prints represent on-tape executions that strategies might use for analytics or VWAP.
struct TradePrint
{
  Price price{0};
  Qty qty{0};
  Side aggressor{Side::Buy};
  u64 ts_ns{0};
};
} // namespace hft
