#pragma once
#include "common/logging.hpp"
#include "matching_engine.hpp"

#include <atomic>
#include <random>
#include <thread>

namespace hft
{
// A tiny exchange simulator that injects random "street" flow to keep the book alive.
// It runs directly inside the engine thread to avoid dealing with multiple producers on queues.
// The goal is pedagogical: expose how external flow alters the book while keeping code compact.
struct StreetFlowConfig
{
  Price mid{10'000}; // ticks
  Price tick{1};
  Qty lot{1};
  double spread_prob{0.6}; // probability asks-bids tighten or widen per step
  double move_prob{0.55};  // probability mid moves by one tick per step
  int max_depth_levels{5}; // depth to seed on start
  u64 seed{42};
};

class Simulator
{
  StreetFlowConfig cfg_;
  std::mt19937_64 rng_;
  std::bernoulli_distribution move_;
  std::bernoulli_distribution widen_;
  u64 next_order_id_{1};
  u64 street_user_{999'999};

public:
  explicit Simulator(StreetFlowConfig cfg = {})
      : cfg_(cfg), rng_(cfg.seed), move_(cfg.move_prob), widen_(1.0 - cfg.spread_prob)
  {
  }

  void seed_book(OrderBook &book)
  {
    // Seed symmetric levels around mid.
    for (int i = 1; i <= cfg_.max_depth_levels; ++i)
    {
      Price bid_px = cfg_.mid - i * cfg_.tick;
      Price ask_px = cfg_.mid + i * cfg_.tick;
      NewOrder b{next_order_id_++, street_user_, Side::Buy, bid_px, 10, TIF::Day, now_ns()};
      NewOrder s{next_order_id_++, street_user_, Side::Sell, ask_px, 10, TIF::Day, now_ns()};
      book.add_passive(b);
      book.add_passive(s);
    }
  }

  // One step of random exogenous flow.
  template <typename Engine> void step(Engine &engine)
  {
    // Randomly choose to lift best ask or hit best bid, or add passive liquidity.
    const bool move_mid = move_(rng_);
    const bool make_spread_wider = widen_(rng_);

    TopOfBook t = engine.top_snapshot();
    Price best_bid = t.bid_price ? t.bid_price : (cfg_.mid - cfg_.tick);
    Price best_ask = t.ask_price ? t.ask_price : (cfg_.mid + cfg_.tick);

    if (move_mid)
    {
      // Marketable order to move mid by one tick. Splitting the branch keeps both sides symmetric.
      if (std::uniform_int_distribution<int>(0, 1)(rng_) == 0)
      {
        // lift ask
        NewOrder m{next_order_id_++, street_user_, Side::Buy, best_ask, 5, TIF::IOC, now_ns()};
        engine.inject_new(m);
      }
      else
      {
        // hit bid
        NewOrder m{next_order_id_++, street_user_, Side::Sell, best_bid, 5, TIF::IOC, now_ns()};
        engine.inject_new(m);
      }
    }
    else
    {
      // Add passive at or near the top. The branch toggles between widening and tightening the
      // spread.
      if (make_spread_wider)
      {
        // Place beyond top to widen
        NewOrder b{next_order_id_++, street_user_, Side::Buy, best_bid - cfg_.tick, 5,
                   TIF::Day,         now_ns()};
        NewOrder s{next_order_id_++, street_user_, Side::Sell, best_ask + cfg_.tick, 5,
                   TIF::Day,         now_ns()};
        engine.inject_new(b);
        engine.inject_new(s);
      }
      else
      {
        // Improve top by one tick each side
        NewOrder b{next_order_id_++, street_user_, Side::Buy, best_bid + cfg_.tick, 5,
                   TIF::Day,         now_ns()};
        NewOrder s{next_order_id_++, street_user_, Side::Sell, best_ask - cfg_.tick, 5,
                   TIF::Day,         now_ns()};
        engine.inject_new(b);
        engine.inject_new(s);
      }
    }
  }
};
} // namespace hft
