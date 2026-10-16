#pragma once
#include "market_types.hpp"
namespace moexwatch {
std::optional<BacktestSummary> run_backtest(const std::vector<Candle>& stock,
                                            const std::vector<Candle>& index,
                                            const SignalSettings& settings,
                                            std::optional<double> target,
                                            double starting_capital,
                                            double commission_percent_per_side);
}
