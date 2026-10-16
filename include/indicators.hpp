#pragma once
#include "market_types.hpp"
namespace moexwatch {
// Missing history or malformed required prices/volumes produce empty optional values.
Analysis analyze(const std::vector<Candle>& candles);
std::vector<std::optional<double>> sma_series(const std::vector<Candle>& candles, std::size_t period);
std::vector<std::optional<double>> rsi_series(const std::vector<Candle>& candles, std::size_t period = 14);
std::vector<std::optional<double>> atr_series(const std::vector<Candle>& candles, std::size_t period = 14);
std::vector<std::optional<double>> volume_ratio_series(const std::vector<Candle>& candles, std::size_t period = 20);
NormalizedComparison normalized_comparison_series(const std::vector<Candle>& stock,
                                                   const std::vector<Candle>& benchmark,
                                                   std::size_t start_index = 0);
}
