#pragma once
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include "types.hpp"

namespace hft
{
// Very small logging helper. Avoids iostreams for lower overhead.
// In real HFT you would batch logs or write to per-thread ring buffers.
inline void log(const char *lvl, const char *fmt, ...) noexcept
{
  std::va_list args;
  va_start(args, fmt);
  std::fputs(lvl, stderr);
  std::fputc(' ', stderr);
  std::fprintf(stderr, "[%llu] ", static_cast<unsigned long long>(now_ns()));
  std::vfprintf(stderr, fmt, args);
  std::fputc('\n', stderr);
  va_end(args);
}

#define HFT_INFO(fmt, ...) ::hft::log("INFO", fmt, ##__VA_ARGS__)
#define HFT_WARN(fmt, ...) ::hft::log("WARN", fmt, ##__VA_ARGS__)
#define HFT_ERROR(fmt, ...) ::hft::log("ERROR", fmt, ##__VA_ARGS__)
} // namespace hft
