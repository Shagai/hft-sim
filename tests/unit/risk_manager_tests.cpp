#include "risk/risk_manager.hpp"

#include <gtest/gtest.h>

namespace hft
{
namespace
{
TEST(RiskManagerTest, CanQuoteRespectsMaxOrderQuantity)
{
  RiskManager risk(100, 1'000'000, 5);
  EXPECT_TRUE(risk.can_quote(5));
  EXPECT_FALSE(risk.can_quote(6));
}

TEST(RiskManagerTest, CanQuoteBlockedWhenPositionLimitZero)
{
  RiskManager risk(/*max_position*/ 0, /*max_notional*/ 1'000'000, /*max_order_qty*/ 5);
  EXPECT_FALSE(risk.can_quote(1));
}

TEST(RiskManagerTest, OnExecAccumulatesNotional)
{
  RiskManager risk(10, 1'000'000, 5);

  ExecEvent traded{};
  traded.type = ExecType::Trade;
  traded.price = 101;
  traded.filled = 3;

  risk.on_exec(traded);
  EXPECT_EQ(risk.notional(), 303);

  risk.on_exec(traded);
  EXPECT_EQ(risk.notional(), 606);
}
} // namespace
} // namespace hft
