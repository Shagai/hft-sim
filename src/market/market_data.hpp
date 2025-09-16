#pragma once
#include <variant>
#include "order.hpp"

namespace hft
{
    using MarketDataEvent = std::variant<TopOfBook, TradePrint>;
} // namespace hft
