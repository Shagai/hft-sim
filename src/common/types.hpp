#pragma once
#include <cstdint>
#include <string_view>
#include <chrono>

// Fundamental types kept small and POD to encourage cache-friendly code.
// Prices and quantities are integers (ticks and lots). Convert at the edge.
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

using Price = i64; // integer ticks
using Qty = i32;   // integer lots

enum class Side : u8
{
  Buy = 0,
  Sell = 1
};

// Monotonic time in nanoseconds for ordering and latency metrics.
// Use steady_clock to avoid wall-clock jumps.
inline u64 now_ns() noexcept
{
  using namespace std::chrono;
  return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

// Simple string identifiers are passed as string_view at API boundary to avoid copies.
using sv = std::string_view;
} // namespace hft
