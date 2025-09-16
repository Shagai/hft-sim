#pragma once
#include <variant>
#include "order.hpp"

namespace hft
{
// Market data feed emitted by the engine: either a snapshot of the inside market (top of book)
// or a trade print. std::variant keeps the interface type-safe and extendable for later lessons.
using MarketDataEvent = std::variant<TopOfBook, TradePrint>;
} // namespace hft
