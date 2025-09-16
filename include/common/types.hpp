#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

// Fundamental types kept small and POD to encourage cache-friendly code.
// The intent is to make units explicit (Price vs Qty) while keeping storage trivial.
// Prices and quantities are integers (ticks and lots). Convert at the edge toward UI/IO layers.
namespace hft
{
using i8 = std::int8_t;
using u8 = std::uint8_t;
using i16 = std::int16_t;
using u16 = std::uint16_t;
using i32 = std::int32_t;
using u32 = std::uint32_t;
using i64 = std::int64_t;
using u64 = std::uint64_t;

// Market data and order handling typically represent prices in integer "ticks" to avoid FP error.
using Price = i64;
// Quantities are small positive integers, rarely exceeding billions in a toy sim.
using Qty = i32;

enum class Side : u8
{
  Buy = 0,
  Sell = 1
};

// Monotonic time in nanoseconds for ordering and latency metrics.
// Steady clock is immune to wall adjustments, providing stable latencies for backtests.
inline u64 now_ns() noexcept
{
  using namespace std::chrono;
  return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

// Simple string identifiers are passed as string_view at API boundary to avoid copies yet remain
// safe.
using sv = std::string_view;
} // namespace hft
