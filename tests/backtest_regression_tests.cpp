#include "backtest.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace moexwatch;
static void check(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}
static std::vector<Candle> history(int n) {
    std::vector<Candle> out;
    for (int i = 0; i < n; ++i) {
        auto date = std::chrono::year_month_day(std::chrono::sys_days(std::chrono::year(2025) / 1 / 1) +
                                                std::chrono::days(i));
        char text[11];
        std::snprintf(text, sizeof(text), "%04d-%02u-%02u", int(date.year()), unsigned(date.month()),
                      unsigned(date.day()));
        double price = 100 + i;
        out.push_back({text, price, price + 1, price - 1, price, 100});
    }
    return out;
}
int main() {
    const auto stock = history(220);
    auto index = history(220);
    // Leave only dates after the stock SMA is available. Computing SMA after
    // alignment would see just 20 rows and incorrectly never enter a trade.
    index.erase(index.begin(), index.begin() + 200);
    SignalSettings rules;
    rules.rsi_oversold_enabled = false;
    rules.near_target_enabled = false;
    rules.above_sma200_enabled = true;
    auto result = run_backtest(stock, index, rules, {}, 10000, 0);
    check(result.has_value(), "gapped benchmark rejected");
    check(result->completed_trades == 1, "benchmark gaps changed stock SMA availability");
    check(std::abs(result->strategy_final_value - 10000.0 * 319.0 / 301.0) < 1e-8,
          "trade did not execute on next shared open");
    auto prefix = history(201);
    auto smallIndex = prefix;
    smallIndex.erase(smallIndex.begin(), smallIndex.begin() + 199);
    const auto shortResult = run_backtest(prefix, smallIndex, rules, {}, 10000, 0);
    check(shortResult && shortResult->completed_trades == 1,
          "SMA200 signal mapped to wrong original position");
    // RSI below zero cannot match; strategy keeps cash even while index rises.
    rules.above_sma200_enabled = false;
    rules.rsi_oversold_enabled = true;
    rules.rsi_oversold_below = 0;
    const auto cash = run_backtest(stock, index, rules, {}, 10000, 0.05);
    check(cash && cash->completed_trades == 0 && cash->strategy_final_value == 10000,
          "impossible rule entered trade");
    return 0;
}
