#include "backtest.hpp"
#include "indicators.hpp"
#include "signal_rules.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
namespace moexwatch {
std::optional<BacktestDividendResult>
run_backtest(const std::vector<Candle> &stock, const std::vector<Candle> &index,
             const SignalSettings &settings, std::optional<double> target,
             double starting_capital, double commission_percent_per_side,
             const std::vector<BacktestCashDividend> &dividends) {
  if (!valid_signal_settings(settings) || !std::isfinite(starting_capital) ||
      starting_capital <= 0.0 || !std::isfinite(commission_percent_per_side) ||
      commission_percent_per_side < 0.0 || commission_percent_per_side > 10.0 ||
      (target && (!std::isfinite(*target) || *target <= 0.0)))
    return std::nullopt;
  const auto date_key = [](const Candle &candle) -> std::string {
    if (candle.date.size() < 10)
      return {};
    return candle.date.substr(0, 10);
  };
  const auto valid_series = [&](const std::vector<Candle> &candles) {
    std::string previous;
    for (const auto &candle : candles) {
      const auto date = date_key(candle);
      if (date.empty() || (!previous.empty() && date <= previous) ||
          !std::isfinite(candle.open) || candle.open <= 0.0 ||
          !std::isfinite(candle.close) || candle.close <= 0.0)
        return false;
      previous = date;
    }
    return true;
  };
  if (stock.size() < 2 || index.size() < 2 || !valid_series(stock) ||
      !valid_series(index))
    return std::nullopt;

  std::map<std::string, Candle> index_by_date;
  for (const auto &candle : index)
    index_by_date.emplace(date_key(candle), candle);
  std::vector<std::pair<Candle, Candle>> common;
  for (const auto &candle : stock) {
    const auto found = index_by_date.find(date_key(candle));
    if (found != index_by_date.end())
      common.emplace_back(candle, found->second);
  }
  if (common.size() < 2)
    return std::nullopt;

  // An index outage must not change the stock's indicators: calculate on its
  // complete chronological history, then look up values on execution dates.
  const auto rsi = rsi_series(stock);
  const auto sma200 = sma_series(stock, 200);
  std::map<std::string, std::size_t> stock_positions;
  for (std::size_t i = 0; i < stock.size(); ++i)
    stock_positions.emplace(date_key(stock[i]), i);
  const auto valid_date = [](const std::string &value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-')
      return false;
    int y{}, m{}, d{};
    const auto parse = [&](int start, int length, int &out) {
      const auto r = std::from_chars(value.data() + start,
                                     value.data() + start + length, out);
      return r.ec == std::errc{} && r.ptr == value.data() + start + length;
    };
    return parse(0, 4, y) && parse(5, 2, m) && parse(8, 2, d) &&
           std::chrono::year_month_day{std::chrono::year{y},
                                       std::chrono::month{unsigned(m)},
                                       std::chrono::day{unsigned(d)}}
               .ok();
  };
  std::set<std::string> event_ids;
  for (const auto &event : dividends) {
    if (!valid_date(event.cutoff_date) || !valid_date(event.payment_date) ||
        event.payment_date < event.cutoff_date || event.source_id.empty() ||
        !event_ids.insert(event.source_id).second ||
        !std::isfinite(event.amount_per_share) || event.amount_per_share <= 0)
      return std::nullopt;
  }
  for (const auto &event : dividends) {
    if (event.cutoff_date >= date_key(common.front().first) &&
        event.cutoff_date <= date_key(common.back().first) &&
        std::none_of(common.begin(), common.end(), [&](const auto &session) {
          return date_key(session.first) == event.cutoff_date;
        }))
      return std::nullopt;
  }
  struct Entitlement {
    double strategy{}, stock{};
    bool captured{}, paid{};
  };
  std::vector<Entitlement> rights(dividends.size());
  BacktestDividendResult extended;
  const double fee = commission_percent_per_side / 100.0;
  double cash = starting_capital;
  double shares = 0.0;
  double peak = starting_capital;
  double max_drawdown = 0.0;
  std::size_t completed = 0;

  const double stock_shares =
      starting_capital / (common.front().first.open * (1.0 + fee));
  const auto process_close = [&](const std::string &date) {
    for (std::size_t j = 0; j < dividends.size(); ++j) {
      const auto &event = dividends[j];
      auto &right = rights[j];
      // Never infer holdings across a missing last-buy session.
      if (!right.captured && date == event.cutoff_date) {
        right.captured = true;
        right.strategy = shares * event.amount_per_share;
        right.stock = stock_shares * event.amount_per_share;
        ++extended.dividend_events;
      }
      if (right.captured && !right.paid && date >= event.payment_date) {
        cash += right.strategy;
        extended.strategy_dividends_received += right.strategy;
        extended.stock_dividends_received += right.stock;
        right.paid = true;
      }
    }
  };
  process_close(date_key(common.front().first));
  for (std::size_t i = 0; i + 1 < common.size(); ++i) {
    Analysis analysis;
    const auto stock_position = stock_positions.at(date_key(common[i].first));
    analysis.rsi14 = rsi[stock_position];
    analysis.sma200 = sma200[stock_position];
    const auto signal = evaluate_signals(analysis, common[i].first.close,
                                         target, false, false, settings);
    // Payments on intervening non-trading dates are cash before this open;
    // a payment dated this session is credited only at its close.
    for (std::size_t j = 0; j < dividends.size(); ++j) {
      auto &right = rights[j];
      if (right.captured && !right.paid &&
          dividends[j].payment_date < date_key(common[i + 1].first)) {
        cash += right.strategy;
        extended.strategy_dividends_received += right.strategy;
        extended.stock_dividends_received += right.stock;
        right.paid = true;
      }
    }
    const double next_open = common[i + 1].first.open;
    if (shares == 0.0 && signal.matched) {
      shares = cash / (next_open * (1.0 + fee));
      cash = 0.0;
    } else if (shares > 0.0 && !signal.matched) {
      cash += shares * next_open * (1.0 - fee);
      shares = 0.0;
      ++completed;
    }
    process_close(date_key(common[i + 1].first));
    const double mark = cash + shares * common[i + 1].first.close * (1.0 - fee);
    peak = std::max(peak, mark);
    if (peak > 0.0)
      max_drawdown = std::max(max_drawdown, (peak - mark) / peak * 100.0);
  }
  if (shares > 0.0) {
    cash += shares * common.back().first.close * (1.0 - fee);
    ++completed;
  }

  const double index_shares =
      starting_capital / (common.front().second.open * (1.0 + fee));
  const double benchmark_final =
      index_shares * common.back().second.close * (1.0 - fee);
  if (!std::isfinite(cash) || !std::isfinite(benchmark_final) || cash <= 0.0 ||
      benchmark_final <= 0.0)
    return std::nullopt;
  BacktestSummary result;
  result.starting_capital = starting_capital;
  result.strategy_final_value = cash;
  result.benchmark_final_value = benchmark_final;
  result.strategy_return_percent = (cash / starting_capital - 1.0) * 100.0;
  result.benchmark_return_percent =
      (benchmark_final / starting_capital - 1.0) * 100.0;
  result.strategy_max_drawdown_percent = max_drawdown;
  result.completed_trades = completed;
  result.start_date = date_key(common.front().first);
  result.end_date = date_key(common.back().first);
  if (!std::isfinite(result.strategy_return_percent) ||
      !std::isfinite(result.benchmark_return_percent) ||
      !std::isfinite(result.strategy_max_drawdown_percent))
    return std::nullopt;
  extended.summary = result;
  for (const auto &right : rights) {
    if (right.captured && !right.paid) {
      extended.strategy_dividends_receivable += right.strategy;
      extended.stock_dividends_receivable += right.stock;
    }
  }
  extended.stock_buy_hold_final_value =
      stock_shares * common.back().first.close * (1.0 - fee) +
      extended.stock_dividends_received;
  extended.stock_buy_hold_return_percent =
      (extended.stock_buy_hold_final_value / starting_capital - 1) * 100;
  for (double value :
       {extended.strategy_dividends_received,
        extended.strategy_dividends_receivable,
        extended.stock_buy_hold_final_value,
        extended.stock_buy_hold_return_percent,
        extended.stock_dividends_received, extended.stock_dividends_receivable})
    if (!std::isfinite(value))
      return std::nullopt;
  return extended;
}

std::optional<BacktestSummary> run_backtest(const std::vector<Candle> &stock,
                                            const std::vector<Candle> &index,
                                            const SignalSettings &settings,
                                            std::optional<double> target,
                                            double capital, double fee) {
  auto result = run_backtest(stock, index, settings, target, capital, fee, {});
  if (!result)
    return std::nullopt;
  return result->summary;
}

} // namespace moexwatch
