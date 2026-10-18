#pragma once
#include "backtest_dividends.hpp"
#include "market_types.hpp"
namespace moexwatch {
std::optional<BacktestSummary>
run_backtest(const std::vector<Candle> &stock, const std::vector<Candle> &index,
             const SignalSettings &settings, std::optional<double> target,
             double starting_capital, double commission_percent_per_side);
std::optional<BacktestDividendResult>
run_backtest(const std::vector<Candle> &stock, const std::vector<Candle> &index,
             const SignalSettings &settings, std::optional<double> target,
             double starting_capital, double commission_percent_per_side,
             const std::vector<BacktestCashDividend> &dividends);
} // namespace moexwatch
