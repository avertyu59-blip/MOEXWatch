#include "signal_rules.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>
namespace moexwatch {
bool matches_filter(const Analysis& analysis, std::optional<double> last, std::optional<double> target,
                    bool stale, WatchFilter filter) {
    if (filter == WatchFilter::All) return true;
    if (stale || !last) return false;
    switch (filter) {
    case WatchFilter::Oversold: return analysis.rsi14 && *analysis.rsi14 < 30.0;
    case WatchFilter::Overbought: return analysis.rsi14 && *analysis.rsi14 > 70.0;
    case WatchFilter::AboveSma200: return analysis.sma200 && *last > *analysis.sma200;
    case WatchFilter::NearTarget: return target && *target > 0.0 && std::abs(*last / *target - 1.0) <= 0.05 + 1e-12;
    case WatchFilter::All: return true;
    }
    return false;
}

bool valid_signal_settings(const SignalSettings& settings) noexcept {
    const auto threshold = [](double value) { return std::isfinite(value) && value >= 0.0 && value <= 100.0; };
    const bool interval_valid = settings.auto_refresh_minutes == 15 || settings.auto_refresh_minutes == 30 ||
                                settings.auto_refresh_minutes == 60;
    return threshold(settings.rsi_oversold_below) && threshold(settings.rsi_overbought_above) &&
           threshold(settings.near_target_percent) && interval_valid;
}

SignalEvaluation evaluate_signals(const Analysis& analysis, std::optional<double> last,
                                  std::optional<double> target, bool quote_stale,
                                  bool candles_stale, const SignalSettings& settings) {
    SignalEvaluation result;
    if (!valid_signal_settings(settings) || quote_stale || candles_stale || !last || !std::isfinite(*last) || *last <= 0.0)
        return result;
    struct Condition { bool matched; std::string reason; };
    std::vector<Condition> conditions;
    const bool has_rsi = analysis.rsi14 && std::isfinite(*analysis.rsi14);
    if (settings.rsi_oversold_enabled) {
        const bool matched = has_rsi && *analysis.rsi14 < settings.rsi_oversold_below;
        std::ostringstream reason;
        reason << std::fixed << std::setprecision(1) << "RSI14 " << (has_rsi ? *analysis.rsi14 : 0.0)
               << " < " << settings.rsi_oversold_below;
        conditions.push_back({matched, reason.str()});
    }
    if (settings.rsi_overbought_enabled) {
        const bool matched = has_rsi && *analysis.rsi14 > settings.rsi_overbought_above;
        std::ostringstream reason;
        reason << std::fixed << std::setprecision(1) << "RSI14 " << (has_rsi ? *analysis.rsi14 : 0.0)
               << " > " << settings.rsi_overbought_above;
        conditions.push_back({matched, reason.str()});
    }
    if (settings.above_sma200_enabled) {
        const bool available = analysis.sma200 && std::isfinite(*analysis.sma200);
        const bool matched = available && *last > *analysis.sma200;
        std::ostringstream reason;
        reason << std::fixed << std::setprecision(2) << "Цена " << *last << (matched ? " выше SMA200 " : " не выше SMA200 ")
               << (available ? *analysis.sma200 : 0.0);
        conditions.push_back({matched, reason.str()});
    }
    if (settings.near_target_enabled) {
        const bool available = target && std::isfinite(*target) && *target > 0.0;
        const double distance = available ? std::abs(*last / *target - 1.0) * 100.0 : 0.0;
        const bool matched = available && distance <= settings.near_target_percent + 1e-12;
        std::ostringstream reason;
        reason << std::fixed << std::setprecision(2) << "До цели " << distance << "% (лимит "
               << settings.near_target_percent << "%)";
        conditions.push_back({matched, reason.str()});
    }
    if (conditions.empty()) return result;
    bool all_available_and_matched = true;
    bool at_least_one_match = false;
    for (const auto& condition : conditions) {
        if (condition.matched) {
            result.reasons.push_back(condition.reason);
        } else {
            all_available_and_matched = false;
        }
        at_least_one_match = at_least_one_match || condition.matched;
    }
    result.matched = settings.require_all ? all_available_and_matched : at_least_one_match;
    if (!result.matched) result.reasons.clear();
    return result;
}



} // namespace moexwatch
