#pragma once
#include "types.hpp"

#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace hft
{
// Very small logging helper. Avoids iostreams for lower overhead and less hidden locks.
// Designed for educational purposes: it keeps formatting familiar while staying close to C APIs.
// Serious deployments would push logs into per-thread buffers and write asynchronously.
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
// Convenience macros to match typical logging ergonomics (INFO/WARN/ERROR).
// They expand to simple printf-style calls keeping call sites compact.
#define HFT_INFO(fmt, ...) ::hft::log("INFO", fmt, ##__VA_ARGS__)
#define HFT_WARN(fmt, ...) ::hft::log("WARN", fmt, ##__VA_ARGS__)
#define HFT_ERROR(fmt, ...) ::hft::log("ERROR", fmt, ##__VA_ARGS__)
} // namespace hft
