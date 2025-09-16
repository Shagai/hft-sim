#pragma once
#include <functional>
#include "order_book.hpp"
#include "market_data.hpp"
#include "common/spsc_queue.hpp"

namespace hft
{
// Commands from strategy into engine thread. Each message is handled synchronously by the engine.
struct EngineCommand
{
  enum class Kind : u8
  {
    New,
    Cancel
  } kind{Kind::New};
  NewOrder new_order{};
  CancelOrder cancel{};
};

// MatchingEngine owns an OrderBook and emits ExecEvents and MarketDataEvents.
// It is intentionally single-threaded: one engine thread reads commands from a queue and calls
// `on_command`. Callers supply SPSC queues for outputs so we keep lock-free semantics end-to-end.
class MatchingEngine
{
  OrderBook &_book;
  spsc::Queue<ExecEvent, 1 << 14> &_exec_out;
  spsc::Queue<MarketDataEvent, 1 << 14> &_md_out;
  u64 _last_trade_ts{0};

public:
  MatchingEngine(OrderBook &book, spsc::Queue<ExecEvent, 1 << 14> &exec_out,
                 spsc::Queue<MarketDataEvent, 1 << 14> &md_out)
      : _book(book), _exec_out(exec_out), _md_out(md_out)
  {
  }

  // Process a NewOrder or CancelOrder. Non-blocking.
  void on_command(const EngineCommand &cmd)
  {
    if (cmd.kind == EngineCommand::Kind::New)
    {
      handle_new(cmd.new_order);
    }
    else
    {
      handle_cancel(cmd.cancel);
    }
  }

private:
  void publish_top()
  {
    TopOfBook t = _book.top();
    _md_out.push(MarketDataEvent{t});
  }

  // Exec events are small enough to pass by value. SPSC queue avoids heap allocations here.
  void send_exec(ExecEvent e)
  {
    _exec_out.push(e);
  }

  void handle_cancel(const CancelOrder &cxl)
  {
    const Qty canceled = _book.cancel(cxl.order_id);
    ExecEvent e{};
    e.ts_ns = now_ns();
    e.order_id = cxl.order_id;
    e.user_id = cxl.user_id;
    if (canceled > 0)
    {
      e.type = ExecType::CancelAck;
      e.leaves = 0;
    }
    else
    {
      e.type = ExecType::Reject;
      e.reason = sv{"unknown order id"};
    }
    send_exec(e);
    publish_top();
  }

  // Core matching loop. Accepts new order, executes against opposite side, then handles residue.
  void handle_new(NewOrder n)
  {
    n.ts_ns = n.ts_ns ? n.ts_ns : now_ns();

    // First match against opposite side.
    Qty remaining =
        _book.match(n,
                    [&](Price px, Qty q, const Order &resting)
                    {
                      ExecEvent trade{};
                      (void)resting; // resting details currently unused but kept for future hooks
                      trade.type = ExecType::Trade;
                      trade.order_id = n.order_id;
                      trade.user_id = n.user_id;
                      trade.price = px;
                      trade.filled = q;
                      trade.leaves = 0; // updated below after loop
                      trade.ts_ns = now_ns();
                      _last_trade_ts = trade.ts_ns;

                      // Send the aggressor trade
                      _exec_out.push(trade);

                      // And a trade print for market data
                      TradePrint tp{px, q, n.side, trade.ts_ns};
                      _md_out.push(MarketDataEvent{tp});
                    });

    // If not fully filled, handle TIF and add passive residue.
    if (remaining > 0)
    {
      if (n.tif == TIF::IOC || n.tif == TIF::FOK)
      {
        // IOC: drop remainder. FOK: reject if anything filled less than full.
        ExecEvent e{};
        e.order_id = n.order_id;
        e.user_id = n.user_id;
        e.ts_ns = now_ns();
        if (n.tif == TIF::FOK && remaining != n.qty)
        {
          e.type = ExecType::Reject;
          e.reason = sv{"FOK not fully filled"};
        }
        else
        {
          e.type = ExecType::Ack; // indicate accepted but no resting (IOC used and nothing left)
          e.leaves = 0;
        }
        send_exec(e);
      }
      else
      {
        NewOrder residue = n;
        residue.qty = remaining;
        _book.add_passive(residue);

        ExecEvent e{};
        e.type = ExecType::Ack;
        e.order_id = n.order_id;
        e.user_id = n.user_id;
        e.leaves = remaining;
        e.ts_ns = now_ns();
        send_exec(e);
      }
    }

    publish_top();
  }
};
} // namespace hft
