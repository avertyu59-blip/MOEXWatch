#include "backtest.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
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
    auto date = std::chrono::year_month_day(
        std::chrono::sys_days(std::chrono::year(2025) / 1 / 1) +
        std::chrono::days(i));
    char text[11];
    std::snprintf(text, sizeof(text), "%04d-%02u-%02u", int(date.year()),
                  unsigned(date.month()), unsigned(date.day()));
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
  check(result->completed_trades == 1,
        "benchmark gaps changed stock SMA availability");
  check(std::abs(result->strategy_final_value - 10000.0 * 319.0 / 301.0) < 1e-8,
        "trade did not execute on next shared open");
  auto prefix = history(201);
  auto smallIndex = prefix;
  smallIndex.erase(smallIndex.begin(), smallIndex.begin() + 199);
  const auto shortResult =
      run_backtest(prefix, smallIndex, rules, {}, 10000, 0);
  check(shortResult && shortResult->completed_trades == 1,
        "SMA200 signal mapped to wrong original position");
  // RSI below zero cannot match; strategy keeps cash even while index rises.
  rules.above_sma200_enabled = false;
  rules.rsi_oversold_enabled = true;
  rules.rsi_oversold_below = 0;
  const auto cash = run_backtest(stock, index, rules, {}, 10000, 0.05);
  check(cash && cash->completed_trades == 0 &&
            cash->strategy_final_value == 10000,
        "impossible rule entered trade");
  rules.rsi_oversold_enabled = false;
  rules.above_sma200_enabled = true;
  auto full = history(220);
  std::vector<BacktestCashDividend> payments{
      {full[205].date, full[210].date, 10, "event1"}};
  const auto base = run_backtest(full, full, rules, {}, 10000, 0);
  const auto empty = run_backtest(full, full, rules, {}, 10000, 0, {});
  check(base && empty &&
            base->strategy_final_value == empty->summary.strategy_final_value,
        "empty dividends changed default test");
  const auto paid = run_backtest(full, full, rules, {}, 10000, 0, payments);
  const double owned = 10000.0 / 300.0;
  check(paid && std::abs(paid->strategy_dividends_received - owned * 10) < 1e-8,
        "cutoff holdings incorrect");
  check(std::abs(paid->summary.strategy_final_value -
                 base->strategy_final_value - owned * 10) < 1e-8,
        "payment reinvested before next session or cash lost");
  auto exitHistory = full;
  // The selected target is matched through cutoff, then the position exits.
  SignalSettings exitRules;
  exitRules.rsi_oversold_enabled = false;
  exitRules.above_sma200_enabled = false;
  exitRules.near_target_enabled = true;
  exitRules.near_target_percent = 0;
  for (auto &candle : exitHistory)
    candle.open = candle.close = 100;
  for (std::size_t i = 206; i < exitHistory.size(); ++i)
    exitHistory[i].open = exitHistory[i].close = 200;
  const auto sold =
      run_backtest(exitHistory, full, exitRules, 100, 10000, 0, payments);
  check(sold && std::abs(sold->strategy_dividends_received - 1000) < 1e-8,
        "sale after cutoff lost dividend entitlement");
  payments[0].payment_date = "2026-01-01";
  const auto future = run_backtest(full, full, rules, {}, 10000, 0, payments);
  check(future &&
            future->summary.strategy_final_value ==
                base->strategy_final_value &&
            future->strategy_dividends_received == 0 &&
            future->strategy_dividends_receivable > 0,
        "future payment entered cash");
  payments[0].payment_date = payments[0].cutoff_date;
  check(run_backtest(full, full, rules, {}, 10000, 0, payments).has_value(),
        "same-day payment rejected");
  payments.push_back(payments[0]);
  check(!run_backtest(full, full, rules, {}, 10000, 0, payments),
        "duplicate payment accepted");
  payments.pop_back();
  payments[0].payment_date = "2024-01-01";
  check(!run_backtest(full, full, rules, {}, 10000, 0, payments),
        "payment before cutoff accepted");
  payments[0].payment_date = "2025-02-30";
  check(!run_backtest(full, full, rules, {}, 10000, 0, payments),
        "invalid calendar date accepted");
  payments[0].payment_date = "2026-01-01";
  payments[0].amount_per_share = std::numeric_limits<double>::infinity();
  check(!run_backtest(full, full, rules, {}, 10000, 0, payments),
        "infinite payment accepted");
  payments[0].amount_per_share = std::numeric_limits<double>::max();
  check(!run_backtest(full, full, rules, {}, 10000, 0, payments),
        "overflow payment accepted");
  return 0;
}
