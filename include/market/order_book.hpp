#pragma once
#include "order.hpp"

#include <deque>
#include <map>
#include <unordered_map>

// A simple price-time priority order book using std::map for clarity.
// Each price level owns a deque of resting orders to preserve FIFO priority.
// This data structure emphasises readability; production engines rely on custom lock-free pools.
namespace hft
{
class OrderBook
{
  using LevelQueue = std::deque<Order>;
  std::map<Price, LevelQueue, std::greater<Price>> _bids;    // highest price first
  std::map<Price, LevelQueue, std::less<Price>> _asks;       // lowest price first
  std::unordered_map<u64, std::pair<Price, Side>> _id_index; // order_id -> (price, side)

  // Compute total quantity at a level without modifying it. Used when forming top-of-book
  // snapshots.
  static Qty total_qty(const LevelQueue &q) noexcept
  {
    Qty tot = 0;
    for (auto const &o : q)
      tot += o.qty;
    return tot;
  }

public:
  TopOfBook top() const noexcept
  {
    // Build a TopOfBook by looking at best bid/ask levels. If any side is empty we leave zeros.
    TopOfBook t{};
    if (!_bids.empty())
    {
      t.bid_price = _bids.begin()->first;
      t.bid_qty = total_qty(_bids.begin()->second);
    }
    if (!_asks.empty())
    {
      t.ask_price = _asks.begin()->first;
      t.ask_qty = total_qty(_asks.begin()->second);
    }
    t.ts_ns = now_ns();
    return t;
  }

  bool empty() const noexcept
  {
    return _bids.empty() && _asks.empty();
  }

  // Insert a new passive order into the book at given price.
  void add_passive(const NewOrder &n)
  {
    // Convert the immutable NewOrder into a mutable resting Order entry.
    Order o{n.order_id, n.user_id, n.side, n.price, n.qty, n.ts_ns};
    if (n.side == Side::Buy)
    {
      auto &q = _bids[n.price];
      q.push_back(o);
    }
    else
    {
      auto &q = _asks[n.price];
      q.push_back(o);
    }
    _id_index.emplace(n.order_id, std::make_pair(n.price, n.side));
  }

  // Cancel by ID. Returns canceled quantity.
  Qty cancel(u64 order_id)
  {
    auto it = _id_index.find(order_id);
    if (it == _id_index.end())
      return 0;
    auto [price, side] = it->second;
    Qty canceled = 0;
    if (side == Side::Buy)
    {
      auto lit = _bids.find(price);
      if (lit != _bids.end())
      {
        auto &q = lit->second;
        for (auto i = q.begin(); i != q.end(); ++i)
        {
          if (i->order_id == order_id)
          {
            canceled = i->qty;
            q.erase(i);
            break;
          }
        }
        if (q.empty())
          _bids.erase(lit);
      }
    }
    else
    {
      auto lit = _asks.find(price);
      if (lit != _asks.end())
      {
        auto &q = lit->second;
        for (auto i = q.begin(); i != q.end(); ++i)
        {
          if (i->order_id == order_id)
          {
            canceled = i->qty;
            q.erase(i);
            break;
          }
        }
        if (q.empty())
          _asks.erase(lit);
      }
    }
    _id_index.erase(it);
    return canceled;
  }

  // Match an aggressive order against the opposite side.
  // Calls the provided on_trade(price, qty, resting_order) for each fill.
  template <typename OnTrade> Qty match(NewOrder aggressive, OnTrade on_trade)
  {
    Qty remaining = aggressive.qty;

    // Crossing condition helper
    auto crosses = [&](Price opp_price) -> bool
    {
      if (aggressive.side == Side::Buy)
        return aggressive.price >= opp_price;
      return aggressive.price <= opp_price;
    };

    auto walk = [&](auto &side)
    {
      for (auto it = side.begin(); it != side.end() && remaining > 0 && crosses(it->first);)
      {
        auto &q = it->second;
        while (!q.empty() && remaining > 0)
        {
          Order &rest = q.front();
          const Qty traded = (remaining < rest.qty) ? remaining : rest.qty;
          // Notify the caller about the trade so it can produce exec/print messages.
          on_trade(it->first, traded, rest);
          rest.qty -= traded;
          remaining -= traded;
          if (rest.qty == 0)
          {
            _id_index.erase(rest.order_id);
            q.pop_front();
          }
        }
        if (q.empty())
          it = side.erase(it);
        else
          ++it;
      }
    };

    if (aggressive.side == Side::Buy)
    {
      walk(_asks);
    }
    else
    {
      walk(_bids);
    }
    return remaining;
  }
};
} // namespace hft
