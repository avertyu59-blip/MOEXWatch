#pragma once
#include "market_types.hpp"
namespace moexwatch {
// cutoff_date is the last purchase date (LASTBUY), not the registry date.
struct BacktestCashDividend {
  std::string cutoff_date, payment_date;
  double amount_per_share{};
  std::string source_id;
};
struct BacktestDividendResult {
  BacktestSummary summary;
  double strategy_dividends_received{}, strategy_dividends_receivable{};
  double stock_buy_hold_final_value{}, stock_buy_hold_return_percent{};
  double stock_dividends_received{}, stock_dividends_receivable{};
  std::size_t dividend_events{};
};
} // namespace moexwatch
