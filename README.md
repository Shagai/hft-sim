# HFT-Sim: Low-Latency C++23 HFT Scaffold + Stock Market Simulator

A compact C++23 codebase that demonstrates an event-driven HFT architecture with a matching engine, a stock market simulator, a sample market-making strategy, and functional tests. The goal is to learn latency-aware design and build upward.

## Build

Requirements:
- CMake ≥ 3.26
- A C++23 compiler (Clang 17+ or GCC 13+ or MSVC 19.39+)

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

Executables:
- `hft_app` — engine + simulator + mean-reversion strategy
- `sim_app` — engine + simulator only
- `sim_scenarios` — functional tests over the simulator

Run:
```bash
./hft_app
./sim_app
./sim_scenarios
```

## Tests & Coverage

Unit tests live under `tests/unit` and rely on GoogleTest/GoogleMock. CMake downloads the
dependency on the first configure step; make sure outbound HTTPS to `github.com` is available or
pre-populate `build/_deps` with a mirror.

```bash
mkdir -p build && cd build
cmake -DHFT_BUILD_TESTS=ON ..
cmake --build . -j
ctest --output-on-failure
```

Enable coverage instrumentation with GCC or Clang by passing `-DHFT_ENABLE_COVERAGE=ON` (Debug
builds work best) and install `gcovr`.

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DHFT_BUILD_TESTS=ON -DHFT_ENABLE_COVERAGE=ON ..
cmake --build . --target coverage
```

Reports are written to `build/coverage/` (`index.html`, `coverage.xml`, and `summary.txt`) and are
suitable for local browsing or CI uploads.

## Architecture

- **common/spsc_queue.hpp**: lock-free SPSC ring buffer (power-of-two capacity). No dynamic allocation on hot path.
- **market/order_book.hpp**: simple price-time book using `std::map`. Clear and correct, not the fastest.
- **market/matching_engine.hpp**: matching core. Emits `ExecEvent` and market data (`TopOfBook`, `TradePrint`).
- **market/simulator.hpp**: seeds depth and injects random exogenous “street” flow to exercise the book.
- **gateway/gateway_sim.hpp**: single engine thread loop that drains strategy commands and runs the simulator.
- **strategy/mean_reversion.hpp**: toy market-making strategy with a rolling mean; quotes around mid.
- **risk/risk_manager.hpp**: minimal per-strategy limits.
- **tests/functional_scenarios.cpp**: black-box scenario against the simulator.

Threads:
- Engine thread: matching + simulator.
- Strategy market-data thread.
- Strategy execs thread.

Queues:
- Strategy → Engine: `EngineCommand` SPSC.
- Engine → Strategy: `ExecEvent` SPSC.
- Engine → Strategy: `MarketDataEvent` SPSC.

## Perf Optimization Tips (practical and incremental)

1. **Data layout**
   - Use SoA for hot structs. Keep messages ≤ 64 bytes when possible.
   - Replace `std::map` book with a flat array-of-levels keyed by tick index. Pre-size contiguous arrays for depth.
   - Avoid `std::variant` in hot paths. Use tagged POD structs or separate queues per type.

2. **Memory & allocation**
   - Zero allocations on the hot path. Pre-allocate ring buffers and free lists for orders.
   - Use `alignas(64)` on hot indices and counters to avoid false sharing.
   - Consider huge pages for large rings or snapshots.

3. **Concurrency**
   - Prefer SPSC queues between fixed pairs. For MPSC, use per-producer SPSC and a combiner.
   - Pin threads with CPU affinity. Keep communicating threads on the same NUMA node.
   - Busy-wait with bounded spin before fallback sleep; use `pthread_setaffinity_np` or `SetThreadAffinityMask`.

4. **Timers and time**
   - Use `steady_clock` or TSC (with calibration) for latency metrics.
   - Batch timers. Avoid system calls in hot loops.

5. **I/O**
   - For real feeds/gateways explore AF_XDP, io_uring, or DPDK for kernel-bypass NIC I/O.
   - Use zero-copy parsing. Avoid string parsing; encode binary.

6. **Compiler & build**
   - Enable `-O3 -march=native -flto` for Release.
   - In Debug, use sanitizers and `-fno-omit-frame-pointer`. Keep logging on only in Debug builds.

7. **Observability**
   - Export counters via a lock-free stats ring or shared memory. Sample from a cold thread.
   - Keep logging out of hot paths. Use binary logs if needed.

8. **Testing and determinism**
   - Seed RNGs. Record inbound messages to a file for replay.
   - Add invariants: monotonic timestamps, non-negative leaves, price-time priority preserved.

9. **Matching engine improvements**
   - Use intrusive lists for FIFO within price levels.
   - Use an index map `order_id -> (level_ptr, node_ptr)` for O(1) cancel.
   - Maintain best-price pointers to avoid map lookups.

10. **Strategy loop**
    - Keep signal computation branch-light. Replace rolling mean with constant-time Welford updates.
    - Co-locate PnL and position in one cache line.

## Testing Strategy

- **Functional tests** (in `sim_scenarios`):
  - Run the engine + simulator + strategy for a fixed time. Assert trades occurred and no invariants broke.
- **Deterministic replays**:
  - Add a recorder for `MarketDataEvent` and `ExecEvent`. Save and replay through the strategy only.
- **Property checks**:
  - Top-of-book must never cross (bid < ask).
  - Trade prices must be within or at the touch.
  - After cancel or full fill, order must disappear from the book.

Extend tests by adding scenarios:
- Tight-spread stress: increase `spread_prob` to keep spread at one tick.
- Illiquid regime: reduce order sizes and event rates.
- Burst traffic: batch-insert 1k orders and assert engine latency bounds.

## Next Implementation Steps

1. Replace `std::map` book with a **flat level book** and intrusive FIFO queues.
2. Add **order persistence** and **O(1) cancel** via id-index to level nodes.
3. Introduce **per-symbol partitioning** and multi-symbol routing.
4. Add **risk checks in the engine** (pre-trade and post-trade) and reject flow when limits hit.
5. Add **metrics**: per-queue depth, per-leg latency p50/p99.
6. Implement **replay** and **deterministic seed** test suites.
7. Create a **real UDP/ITCH** stub gateway or **AF_XDP** demo for raw NIC I/O (Linux).

## Notes

- The simulator and matching engine live in the same thread to keep queues SPSC-only.
- Code avoids exceptions and RTTI by default. Remove the flags if you add code that relies on them.
- This is a learning scaffold. It favors clarity in a few places over raw speed. Replace parts as you progress.
