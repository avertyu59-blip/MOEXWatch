#pragma once
// Compatibility facade; calculation clients can include narrower headers.
#include "market_types.hpp"
#include "indicators.hpp"
#include "portfolio_calculations.hpp"
#include "signal_rules.hpp"
#include "backtest.hpp"
#include "dividend_calculations.hpp"
#include "domain_common.hpp"

namespace moexwatch {
// Invalid provider responses throw; a recognized empty table is a valid empty result.
std::vector<DividendEvent> parse_smartlab_dividends(std::string_view html, std::string_view ticker,
                                                 std::string_view source_url);
std::vector<DividendEvent> parse_lukoil_dividends(std::string_view html);
void reconcile_dividends(std::vector<DividendEvent>& events, const std::vector<DividendEvent>& official,
                         std::string_view issuer_url, std::string_view checked_at);
void merge_dividends(std::vector<DividendEvent>& events, const std::vector<DividendEvent>& incoming);
std::vector<TradingDay> moex_calendar_2026();
std::string serialize_dividend_snapshot(const DividendSnapshot& snapshot);
std::optional<DividendSnapshot> parse_dividend_snapshot(std::string_view csv, std::string_view ticker);
std::string dividend_status_text(DividendStatus status);
std::optional<Quote> parse_quote_csv(std::string_view csv, std::string_view ticker);
std::vector<Candle> parse_candles_csv(std::string_view csv);
std::vector<DividendEvent> parse_dividends_csv(std::string_view csv);
bool has_candle_header(std::string_view csv);
std::vector<std::vector<std::string>> parse_csv(std::string_view csv, char delimiter = ';');
}
