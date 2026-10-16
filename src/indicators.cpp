#include "indicators.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>
namespace moexwatch {
namespace {
bool valid_closes(const std::vector<Candle>& candles) {
    return std::all_of(candles.begin(), candles.end(), [](const auto& c) { return std::isfinite(c.close) && c.close > 0.0; });
}
std::string date_key(const Candle& candle) { return candle.date.size() < 10 ? std::string{} : candle.date.substr(0,10); }
}
std::vector<std::optional<double>> sma_series(const std::vector<Candle>& candles, std::size_t period) {
    std::vector<std::optional<double>> values(candles.size());
    if (period == 0 || !valid_closes(candles)) return values;
    double sum = 0.0;
    for (std::size_t i = 0; i < candles.size(); ++i) {
        sum += candles[i].close;
        if (i >= period) sum -= candles[i - period].close;
        if (i + 1 >= period) values[i] = sum / static_cast<double>(period);
    }
    return values;
}

std::vector<std::optional<double>> rsi_series(const std::vector<Candle>& candles, std::size_t period) {
    std::vector<std::optional<double>> values(candles.size());
    if (period == 0 || candles.size() <= period || !valid_closes(candles)) return values;
    double gain = 0.0, loss = 0.0;
    for (std::size_t i = 1; i <= period; ++i) {
        const double change = candles[i].close - candles[i - 1].close;
        gain += std::max(change, 0.0); loss += std::max(-change, 0.0);
    }
    const double length = static_cast<double>(period);
    gain /= length; loss /= length;
    const auto value = [&]() { return loss == 0.0 ? (gain == 0.0 ? 50.0 : 100.0) : 100.0 - 100.0 / (1.0 + gain / loss); };
    values[period] = value();
    for (std::size_t i = period + 1; i < candles.size(); ++i) {
        const double change = candles[i].close - candles[i - 1].close;
        gain = (gain * (length - 1.0) + std::max(change, 0.0)) / length;
        loss = (loss * (length - 1.0) + std::max(-change, 0.0)) / length;
        values[i] = value();
    }
    return values;
}

std::vector<std::optional<double>> atr_series(const std::vector<Candle>& candles, std::size_t period) {
    std::vector<std::optional<double>> values(candles.size());
    if (period == 0 || candles.size() < period || !valid_closes(candles) ||
        !std::all_of(candles.begin(), candles.end(), [](const auto& c) {
            return std::isfinite(c.high) && std::isfinite(c.low) && c.low > 0.0 && c.high >= c.low;
        })) return values;

    const auto true_range = [&](std::size_t index) {
        const auto& candle = candles[index];
        if (index == 0) return candle.high - candle.low;
        const double previous_close = candles[index - 1].close;
        return std::max({candle.high - candle.low,
                         std::abs(candle.high - previous_close),
                         std::abs(candle.low - previous_close)});
    };

    const double length = static_cast<double>(period);
    double average = 0.0;
    for (std::size_t i = 0; i < period; ++i) average += true_range(i);
    average /= length;
    values[period - 1] = average;
    for (std::size_t i = period; i < candles.size(); ++i) {
        average = (average * (length - 1.0) + true_range(i)) / length;
        values[i] = average;
    }
    return values;
}

std::vector<std::optional<double>> volume_ratio_series(const std::vector<Candle>& candles, std::size_t period) {
    std::vector<std::optional<double>> values(candles.size());
    if (period == 0 || candles.size() <= period ||
        !std::all_of(candles.begin(), candles.end(), [](const auto& c) { return std::isfinite(c.volume) && c.volume >= 0.0; })) return values;

    double sum = 0.0;
    for (std::size_t i = 0; i < candles.size(); ++i) {
        if (i > period) sum -= candles[i - period - 1].volume;
        if (i > 0) sum += candles[i - 1].volume;
        if (i >= period) {
            const double average = sum / static_cast<double>(period);
            if (average > 0.0 && candles[i].volume >= 0.0)
                values[i] = candles[i].volume / average;
        }
    }
    return values;
}

NormalizedComparison normalized_comparison_series(const std::vector<Candle>& stock,
                                                   const std::vector<Candle>& benchmark,
                                                   std::size_t start_index) {
    NormalizedComparison result{std::vector<std::optional<double>>(stock.size()),
                                std::vector<std::optional<double>>(stock.size())};
    if (start_index >= stock.size()) return result;

    std::map<std::string, double> benchmark_by_date;
    for (const auto& candle : benchmark) {
        const auto key = date_key(candle);
        if (!key.empty() && std::isfinite(candle.close) && candle.close > 0.0)
            benchmark_by_date.try_emplace(key, candle.close);
    }

    std::optional<double> stock_base;
    std::optional<double> benchmark_base;
    for (std::size_t i = start_index; i < stock.size(); ++i) {
        const auto key = date_key(stock[i]);
        const auto found = benchmark_by_date.find(key);
        if (key.empty() || found == benchmark_by_date.end() ||
            !std::isfinite(stock[i].close) || stock[i].close <= 0.0) continue;
        stock_base = stock[i].close;
        benchmark_base = found->second;
        break;
    }
    if (!stock_base || !benchmark_base) return result;

    for (std::size_t i = start_index; i < stock.size(); ++i) {
        const auto key = date_key(stock[i]);
        const auto found = benchmark_by_date.find(key);
        if (key.empty() || found == benchmark_by_date.end() ||
            !std::isfinite(stock[i].close) || stock[i].close <= 0.0) continue;
        result.stock[i] = stock[i].close / *stock_base * 100.0;
        result.benchmark[i] = found->second / *benchmark_base * 100.0;
    }
    return result;
}

Analysis analyze(const std::vector<Candle>& candles) {
    Analysis result;
    if (candles.empty()) { result.technical_note = "Недостаточно данных"; return result; }
    if (!valid_closes(candles)) { result.technical_note = "Некорректные цены свечей"; return result; }
    result.sma20 = sma_series(candles, 20).back();
    result.sma50 = sma_series(candles, 50).back();
    result.sma200 = sma_series(candles, 200).back();
    result.rsi14 = rsi_series(candles).back();
    result.atr14 = atr_series(candles).back();
    if (result.atr14 && candles.back().close > 0.0)
        result.atr_percent = *result.atr14 / candles.back().close * 100.0;
    if (candles.size() >= 2 && candles.front().close > 0.0)
        result.period_change_percent = (candles.back().close / candles.front().close - 1.0) * 100.0;
    result.volume_ratio20 = volume_ratio_series(candles).back();
    double peak = candles.front().high;
    for (const auto& candle : candles) peak = std::max(peak, candle.high);
    if (peak > 0.0) result.drawdown_percent = (candles.back().close / peak - 1.0) * 100.0;
    if (!result.rsi14) result.technical_note = "Для RSI14 нужно не менее 15 свечей";
    else if (*result.rsi14 > 70.0) result.technical_note = "Перекупленность: RSI14 > 70";
    else if (*result.rsi14 < 30.0) result.technical_note = "Перепроданность: RSI14 < 30";
    else result.technical_note = "RSI14 в диапазоне 30–70";
    return result;
}


} // namespace moexwatch
