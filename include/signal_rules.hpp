#pragma once
#include "market_types.hpp"
namespace moexwatch {
bool matches_filter(const Analysis& analysis, std::optional<double> last, std::optional<double> target,
                    bool stale, WatchFilter filter);
SignalEvaluation evaluate_signals(const Analysis& analysis, std::optional<double> last,
                                  std::optional<double> target, bool quote_stale,
                                  bool candles_stale, const SignalSettings& settings);
bool valid_signal_settings(const SignalSettings& settings) noexcept;
}
