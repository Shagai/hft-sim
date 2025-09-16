#pragma once
#include <cstdint>
#include <unordered_map>
#include <map>
#include <deque>
#include "common/types.hpp"

namespace hft
{
    // Thin structs that carry only what the engine needs.
    struct Order
    {
        u64        order_id;
        u64        user_id;
        Side       side;
        Price      price; // ticks
        Qty        qty;   // remaining qty
        u64        ts_ns; // submit time
    };

    enum class TIF : u8 { Day = 0, IOC = 1, FOK = 2 };

    struct NewOrder
    {
        u64  order_id;
        u64  user_id;
        Side side;
        Price price;
        Qty   qty;
        TIF   tif{TIF::Day};
        u64   ts_ns{0};
    };

    struct CancelOrder
    {
        u64 order_id;
        u64 user_id;
        u64 ts_ns{0};
    };

    // Minimal execution report types from engine to strategy.
    enum class ExecType : u8 { Ack, Trade, CancelAck, Reject, DoneForDay };

    struct ExecEvent
    {
        ExecType type{ExecType::Ack};
        u64      order_id{0};
        u64      user_id{0};
        Qty      filled{0};   // for Trade
        Price    price{0};    // for Trade
        Qty      leaves{0};   // remaining
        sv       reason{};    // for Reject
        u64      ts_ns{0};
    };

    // Market data events pushed to strategies. Keep it tiny for cache efficiency.
    struct TopOfBook
    {
        Price bid_price{0};
        Qty   bid_qty{0};
        Price ask_price{0};
        Qty   ask_qty{0};
        u64   ts_ns{0};
    };

    struct TradePrint
    {
        Price price{0};
        Qty   qty{0};
        Side  aggressor{Side::Buy};
        u64   ts_ns{0};
    };
} // namespace hft
