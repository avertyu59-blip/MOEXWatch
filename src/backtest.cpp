#include "backtest.hpp"
#include "indicators.hpp"
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
std::optional<BacktestSummary> run_backtest(const std::vector<Candle>& stock,
                                            const std::vector<Candle>& index,
                                            const SignalSettings& settings,
                                            std::optional<double> target,
                                            double starting_capital,
                                            double commission_percent_per_side) {
    if (!valid_signal_settings(settings) || !std::isfinite(starting_capital) || starting_capital <= 0.0 ||
        !std::isfinite(commission_percent_per_side) || commission_percent_per_side < 0.0 ||
        commission_percent_per_side > 10.0 ||
        (target && (!std::isfinite(*target) || *target <= 0.0))) return std::nullopt;
    const auto date_key = [](const Candle& candle) -> std::string {
        if (candle.date.size() < 10) return {};
        return candle.date.substr(0, 10);
    };
    const auto valid_series = [&](const std::vector<Candle>& candles) {
        std::string previous;
        for (const auto& candle : candles) {
            const auto date = date_key(candle);
            if (date.empty() || (!previous.empty() && date <= previous) ||
                !std::isfinite(candle.open) || candle.open <= 0.0 ||
                !std::isfinite(candle.close) || candle.close <= 0.0) return false;
            previous = date;
        }
        return true;
    };
    if (stock.size() < 2 || index.size() < 2 || !valid_series(stock) || !valid_series(index)) return std::nullopt;

    std::map<std::string, Candle> index_by_date;
    for (const auto& candle : index) index_by_date.emplace(date_key(candle), candle);
    std::vector<std::pair<Candle, Candle>> common;
    for (const auto& candle : stock) {
        const auto found = index_by_date.find(date_key(candle));
        if (found != index_by_date.end()) common.emplace_back(candle, found->second);
    }
    if (common.size() < 2) return std::nullopt;

    std::vector<Candle> aligned_stock;
    aligned_stock.reserve(common.size());
    for (const auto& pair : common) aligned_stock.push_back(pair.first);
    const auto rsi = rsi_series(aligned_stock);
    const auto sma200 = sma_series(aligned_stock, 200);
    const double fee = commission_percent_per_side / 100.0;
    double cash = starting_capital;
    double shares = 0.0;
    double peak = starting_capital;
    double max_drawdown = 0.0;
    std::size_t completed = 0;

    for (std::size_t i = 0; i + 1 < common.size(); ++i) {
        Analysis analysis;
        analysis.rsi14 = rsi[i];
        analysis.sma200 = sma200[i];
        const auto signal = evaluate_signals(analysis, common[i].first.close, target, false, false, settings);
        const double next_open = common[i + 1].first.open;
        if (shares == 0.0 && signal.matched) {
            shares = cash / (next_open * (1.0 + fee));
            cash = 0.0;
        } else if (shares > 0.0 && !signal.matched) {
            cash = shares * next_open * (1.0 - fee);
            shares = 0.0;
            ++completed;
        }
        const double mark = cash + shares * common[i + 1].first.close * (1.0 - fee);
        peak = std::max(peak, mark);
        if (peak > 0.0) max_drawdown = std::max(max_drawdown, (peak - mark) / peak * 100.0);
    }
    if (shares > 0.0) {
        cash = shares * common.back().first.close * (1.0 - fee);
        ++completed;
    }

    const double index_shares = starting_capital / (common.front().second.open * (1.0 + fee));
    const double benchmark_final = index_shares * common.back().second.close * (1.0 - fee);
    if (!std::isfinite(cash) || !std::isfinite(benchmark_final) || cash <= 0.0 || benchmark_final <= 0.0)
        return std::nullopt;
    BacktestSummary result;
    result.starting_capital = starting_capital;
    result.strategy_final_value = cash;
    result.benchmark_final_value = benchmark_final;
    result.strategy_return_percent = (cash / starting_capital - 1.0) * 100.0;
    result.benchmark_return_percent = (benchmark_final / starting_capital - 1.0) * 100.0;
    result.strategy_max_drawdown_percent = max_drawdown;
    result.completed_trades = completed;
    result.start_date = date_key(common.front().first);
    result.end_date = date_key(common.back().first);
    return result;
}


} // namespace moexwatch
