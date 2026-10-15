#include "market_core.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace moexwatch;

void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

int main() {
    const auto block_quote = parse_quote_csv("marketdata\r\n\r\nSECID;LAST;LASTCHANGEPRCNT;UPDATETIME\r\nSBER;286.07;0.04;21:58:53\r\n\r\n", "SBER");
    check(block_quote && block_quote->last && std::abs(*block_quote->last - 286.07) < 1e-9, "ISS block prefix handled");
    check(!parse_quote_csv("marketdata\n\nSBER;286.07;0.04;21:58:53\n", "SBER"), "headerless response rejected");
    const auto block_candles = parse_candles_csv("candles\n\nbegin;open;high;low;close;volume\n2026-06-16 00:00:00;324.7;324.9;318.15;320.25;32508771\n");
    check(block_candles.size() == 1 && block_candles.front().close == 320.25, "ISS candle block prefix handled");
    check(has_candle_header("candles\n\nbegin;open;high;low;close;volume\n"), "valid empty final page");
    check(!has_candle_header("<html>service unavailable</html>"), "invalid page not treated as end of history");
    const auto quote = parse_quote_csv("SECID;LAST;LASTCHANGEPRCNT;UPDATETIME\nSBER;312.4;1.25;18:40:00\n", "SBER");
    check(quote && quote->last && std::abs(*quote->last - 312.4) < 1e-9, "quote last parsed");
    check(quote->change_percent && std::abs(*quote->change_percent - 1.25) < 1e-9, "quote change parsed");
    const auto missing_quote = parse_quote_csv("SECID;LAST\nSBER;\n", "SBER");
    check(missing_quote && !missing_quote->last, "empty quote remains missing");
    const auto parsed = parse_candles_csv("begin;open;high;low;close;volume\n2026-01-01;100;103;99;101;10\n2026-01-02;101;104;100;103;12\n");
    check(parsed.size() == 2 && parsed[1].close == 103.0, "candles parsed");
    const auto a = analyze(parsed);
    check(a.period_change_percent && std::abs(*a.period_change_percent - (103.0 / 101.0 - 1.0) * 100.0) < 1e-8, "period change calculated");
    check(!a.sma20 && !a.rsi14, "indicators omitted for short history");
    std::vector<Candle> trend;
    for (int i = 1; i <= 30; ++i) trend.push_back({"date", static_cast<double>(i), static_cast<double>(i), static_cast<double>(i), static_cast<double>(i), 1.0});
    const auto indicators = analyze(trend);
    check(indicators.sma20 && std::abs(*indicators.sma20 - 20.5) < 1e-9, "SMA20 calculated");
    check(indicators.rsi14 && std::abs(*indicators.rsi14 - 100.0) < 1e-9, "RSI14 calculated for rising prices");
    std::vector<Candle> reference;
    for (double close : {44.34,44.09,44.15,43.61,44.33,44.83,45.10,45.42,45.84,46.08,45.89,46.03,45.61,46.28,46.28,46.00})
        reference.push_back({"date",close,close,close,close,1.0});
    const auto wilder = rsi_series(reference);
    check(!wilder[13] && wilder[14] && std::abs(*wilder[14] - 70.464135) < 1e-5, "Wilder initial RSI reference");
    check(wilder[15] && std::abs(*wilder[15] - 66.249619) < 1e-5, "Wilder recursive RSI reference");
    std::vector<Candle> long_history;
    for (int i=1; i<=250; ++i) long_history.push_back({"date",double(i),double(i),double(i),double(i),100.0});
    auto long_analysis = analyze(long_history);
    check(long_analysis.sma50 && std::abs(*long_analysis.sma50 - 225.5) < 1e-9, "SMA50 history");
    check(long_analysis.sma200 && std::abs(*long_analysis.sma200 - 150.5) < 1e-9, "SMA200 history");
    check(!sma_series(long_history,200)[198] && sma_series(long_history,200)[199], "SMA200 warmup");
    check(long_analysis.volume_ratio20 && *long_analysis.volume_ratio20 == 1.0, "volume baseline excludes current bar");
    std::vector<Candle> atr_fixture{
        {"2026-01-01",10,12,8,10,100},
        {"2026-01-02",10,14,9,11,100},
        {"2026-01-03",11,13,10,12,100}};
    const auto atr = atr_series(atr_fixture, 2);
    check(!atr[0] && atr[1] && std::abs(*atr[1] - 4.5) < 1e-9, "ATR initial true-range average");
    check(atr[2] && std::abs(*atr[2] - 3.75) < 1e-9, "ATR Wilder smoothing");
    check(!atr_series(atr_fixture, 0).back() && !atr_series(atr_fixture, 4).back(), "ATR guards invalid and short periods");
    std::vector<Candle> volume_fixture(21, {"2026-01-01",100,100,100,100,100});
    volume_fixture.back().volume = 300;
    const auto volume_ratio = volume_ratio_series(volume_fixture);
    check(volume_ratio.back() && std::abs(*volume_ratio.back() - 3.0) < 1e-9,
          "volume ratio compares current bar to prior 20 sessions");
    const std::vector<Candle> comparison_stock{
        {"2026-01-01 00:00:00",100,100,100,100,1},
        {"2026-01-02 00:00:00",105,105,105,105,1},
        {"2026-01-03 00:00:00",120,120,120,120,1}};
    const std::vector<Candle> comparison_index{
        {"2026-01-01 00:00:00",3000,3000,3000,3000,1},
        {"2026-01-02 00:00:00",3300,3300,3300,3300,1},
        {"2026-01-03 00:00:00",3300,3300,3300,3300,1}};
    const auto comparison = normalized_comparison_series(comparison_stock, comparison_index, 1);
    check(!comparison.stock[0] && comparison.stock[1] && comparison.benchmark[1] &&
          *comparison.stock[1] == 100.0 && *comparison.benchmark[1] == 100.0,
          "normalized comparison rebases both series at visible range start");
    check(comparison.stock[2] && comparison.benchmark[2] &&
          std::abs(*comparison.stock[2] - 100.0 * 120.0 / 105.0) < 1e-9 &&
          *comparison.benchmark[2] == 100.0,
          "normalized comparison aligns daily candles by date");
    const auto unmatched = normalized_comparison_series(comparison_stock, {}, 0);
    check(std::none_of(unmatched.stock.begin(), unmatched.stock.end(), [](const auto& v) { return v.has_value(); }),
          "normalized comparison handles missing benchmark history");
    check(matches_filter(long_analysis,250.0,250.0,false,WatchFilter::NearTarget), "target filter");
    check(!matches_filter(long_analysis,250.0,250.0,true,WatchFilter::NearTarget), "stale quote excluded from filter");
    check(matches_filter(long_analysis,250.0,{},false,WatchFilter::AboveSma200), "above SMA200 filter");
    check(!matches_filter(long_analysis,{}, {},false,WatchFilter::AboveSma200), "missing quote excluded from filter");
    for (auto& candle : long_history) candle.close = 100.0;
    const auto flat_rsi = rsi_series(long_history);
    check(flat_rsi.back() && *flat_rsi.back() == 50.0, "flat RSI convention");
    check(sma_series(long_history,0).size()==long_history.size() && !sma_series(long_history,0).back(), "zero period guarded");
    check(valid_ticker("YDEX") && !valid_ticker("SBER&evil"), "ticker allowlist");
    const Trade winning{"SBER", 10.0, 100.0, 120.0, 10.0, 13.0};
    const auto winning_result = calculate_trade(winning);
    check(winning_result && std::abs(winning_result->gross_profit - 200.0) < 1e-9, "trade gross profit");
    check(winning_result && std::abs(winning_result->profit_after_commission - 190.0) < 1e-9, "commission deducted");
    check(winning_result && std::abs(winning_result->tax - 24.7) < 1e-9, "tax on positive profit after commission");
    check(winning_result && std::abs(winning_result->net_profit - 165.3) < 1e-9, "trade net profit");
    const std::vector<PortfolioTransaction> portfolio_transactions{
        {"2026-02-02", "SBER", PortfolioSide::Buy, 5.0, 280.0, 4.0},
        {"2026-02-03", "SBER", PortfolioSide::Sell, 8.0, 300.0, 6.0},
        {"2026-02-01", "SBER", PortfolioSide::Buy, 10.0, 250.0, 5.0}};
    const auto portfolio = calculate_portfolio(portfolio_transactions, {Quote{"SBER",270.0,{},"12:00"}});
    check(portfolio && portfolio->open_positions == 1 && portfolio->priced_positions == 1,
          "portfolio counts an open priced position");
    check(portfolio && portfolio->positions.size() == 1 && std::abs(portfolio->positions[0].quantity - 7.0) < 1e-9,
          "portfolio chronologically applies buys and partial sale");
    check(portfolio && portfolio->positions[0].opened_on == "2026-02-01",
          "open position identity starts on the first buy");
    check(portfolio && std::abs(portfolio->positions[0].average_entry_price - 260.6) < 1e-9,
          "weighted average cost includes buy commissions");
    check(portfolio && std::abs(portfolio->positions[0].unrealized_profit.value_or(0.0) - 65.8) < 1e-9,
          "portfolio marks remaining shares to current quote");
    check(portfolio && std::abs(portfolio->positions[0].realized_profit - 309.2) < 1e-9,
          "realized profit deducts sale commission and average cost basis");
    check(portfolio && std::abs(portfolio->total_market_value - 1890.0) < 1e-9 &&
          std::abs(portfolio->total_realized_profit - 309.2) < 1e-9,
          "portfolio aggregates market value and realized profit");
    check(!calculate_portfolio({{"2026-02-01","SBER",PortfolioSide::Sell,1.0,300.0,0.0}},{}),
          "portfolio rejects sale before shares are owned");
    check(!valid_portfolio_transaction(PortfolioTransaction{"2026-02-30","SBER",PortfolioSide::Buy,1.0,100.0,0.0}),
          "portfolio validates calendar dates");
    const PositionLevels configured_levels{"SBER","2026-02-01",240.0,340.0};
    check(valid_position_levels(configured_levels), "position levels accept valid positive prices");
    check(!valid_position_levels(PositionLevels{"SBER","2026-02-01",0.0,340.0}),
          "position levels reject non-positive prices");
    check(!valid_position_levels(PositionLevels{"SBER","2026-02-30",240.0,340.0}),
          "position levels validate opening date");
    const std::vector<PortfolioTransaction> position_lifecycle{
        {"2026-03-01","SBER",PortfolioSide::Buy,2.0,100.0,0.0},
        {"2026-03-03","SBER",PortfolioSide::Buy,1.0,110.0,0.0},
        {"2026-03-04","SBER",PortfolioSide::Sell,1.0,120.0,0.0},
        {"2026-03-05","SBER",PortfolioSide::Sell,2.0,120.0,0.0},
        {"2026-03-06","SBER",PortfolioSide::Buy,4.0,130.0,0.0}};
    const auto reopened = calculate_portfolio(position_lifecycle, {});
    check(reopened && reopened->positions[0].quantity == 4.0 &&
          reopened->positions[0].opened_on == "2026-03-06",
          "full close and later buy starts a new position identity");
    const auto unpriced_portfolio = calculate_portfolio({{"2026-02-01","GAZP",PortfolioSide::Buy,2.0,100.0,1.0}},{});
    check(unpriced_portfolio && unpriced_portfolio->open_positions == 1 && unpriced_portfolio->priced_positions == 0 &&
          !unpriced_portfolio->positions[0].market_value,
          "portfolio keeps open positions visible while quote is unavailable");

    const auto losing_result = calculate_trade(Trade{"SBER", 10.0, 120.0, 100.0, 10.0, 13.0});
    check(losing_result && std::abs(losing_result->tax) < 1e-9 && std::abs(losing_result->net_profit + 210.0) < 1e-9,
          "loss does not produce estimated tax");
    check(!calculate_trade(Trade{"SBER", 0.0, 100.0, 110.0, 0.0, 13.0}), "invalid trade rejected");
    check(!calculate_trade(Trade{"SBER", 1.0, 100.0, 110.0, 0.0, 101.0}), "tax rate over 100 rejected");
    SignalSettings signal_settings;
    Analysis signal_analysis; signal_analysis.rsi14 = 25.0; signal_analysis.sma200 = 100.0;
    const auto any_signals = evaluate_signals(signal_analysis, 104.0, 105.0, false, false, signal_settings);
    check(any_signals.matched && any_signals.reasons.size() == 3, "any mode reports every matching signal reason");
    check(any_signals.reasons[0] == "RSI14 25.0 < 30.0" && any_signals.reasons[2] == "До цели 0.95% (лимит 5.00%)",
          "signal reasons include the observed value and configured threshold");
    signal_settings.require_all = true;
    const auto all_signals = evaluate_signals(signal_analysis, 104.0, 105.0, false, false, signal_settings);
    check(all_signals.matched && all_signals.reasons.size() == 3, "all mode matches when every enabled condition is true");
    const auto incomplete_signals = evaluate_signals(signal_analysis, 104.0, {}, false, false, signal_settings);
    check(!incomplete_signals.matched && incomplete_signals.reasons.empty(), "all mode rejects unavailable condition");
    signal_settings.require_all = false;
    check(!evaluate_signals(signal_analysis, 104.0, 105.0, true, false, signal_settings).matched, "stale quote suppresses signals");
    check(!evaluate_signals(signal_analysis, 104.0, 105.0, false, true, signal_settings).matched, "stale history suppresses signals");
    signal_settings.above_sma200_enabled = false; signal_settings.near_target_enabled = false;
    const auto single_any = evaluate_signals(signal_analysis, 104.0, {}, false, false, signal_settings);
    check(single_any.matched && single_any.reasons.size() == 1, "any mode matches one active condition");
    signal_settings.require_all = true;
    check(evaluate_signals(signal_analysis, 104.0, {}, false, false, signal_settings).matched, "all mode accepts one active condition");
    signal_settings.rsi_oversold_enabled = false; signal_settings.above_sma200_enabled = false;
    signal_settings.near_target_enabled = false;
    check(!evaluate_signals(signal_analysis, 104.0, 105.0, false, false, signal_settings).matched, "no enabled conditions means no signal");
    signal_settings.near_target_percent = 101.0;
    check(!valid_signal_settings(signal_settings), "signal thresholds validated");
    signal_settings = SignalSettings{};
    signal_settings.auto_refresh_minutes = 2;
    check(!valid_signal_settings(signal_settings), "automatic refresh interval validated");
    SignalSettings backtest_settings;
    backtest_settings.rsi_oversold_enabled = false;
    backtest_settings.rsi_overbought_enabled = false;
    backtest_settings.above_sma200_enabled = false;
    backtest_settings.near_target_enabled = true;
    backtest_settings.near_target_percent = 0.0;
    std::vector<Candle> stock_test{
        {"2026-01-01", 100, 100, 100, 100, 1},
        {"2026-01-02", 200, 200, 200, 200, 1},
        {"2026-01-03", 300, 300, 300, 300, 1},
        {"2026-01-04", 300, 300, 300, 300, 1}};
    std::vector<Candle> index_test{
        {"2026-01-01", 100, 100, 100, 100, 0},
        {"2026-01-02", 100, 100, 100, 100, 0},
        {"2026-01-03", 100, 100, 100, 100, 0},
        {"2026-01-04", 100, 100, 100, 100, 0}};
    const auto backtest = run_backtest(stock_test, index_test, backtest_settings, 100.0, 1000.0, 1.0);
    check(backtest && backtest->completed_trades == 1, "backtest enters after close signal and exits next open");
    check(backtest && std::abs(backtest->strategy_final_value - (1000.0 / 202.0 * 297.0)) < 1e-7,
          "backtest applies entry and exit commission and does not use signal-day close as fill");
    check(backtest && std::abs(backtest->benchmark_final_value - (1000.0 / 101.0 * 99.0)) < 1e-7,
          "buy and hold benchmark applies round-trip commission");
    check(backtest && backtest->start_date == "2026-01-01" && backtest->end_date == "2026-01-04",
          "backtest uses common trading dates");
    check(!run_backtest(stock_test, index_test, backtest_settings, 100.0, 0.0, 0.05), "backtest rejects zero capital");
    std::vector<Candle> unmatched_index{{"2025-12-01", 100, 100, 100, 100, 0}, {"2025-12-02", 100, 100, 100, 100, 0}};
    check(!run_backtest(stock_test, unmatched_index, backtest_settings, 100.0, 1000.0, 0.05), "backtest requires overlapping sessions");

    signal_settings = SignalSettings{};
    signal_settings.rsi_oversold_enabled = false; signal_settings.rsi_overbought_enabled = true;
    signal_settings.above_sma200_enabled = false; signal_settings.near_target_enabled = false;
    signal_analysis.rsi14 = 75.0;
    const auto overbought_signal = evaluate_signals(signal_analysis, 104.0, {}, false, false, signal_settings);
    check(overbought_signal.matched && overbought_signal.reasons.size() == 1, "overbought RSI condition");
    const auto quoted = parse_csv("A;B\n1;\"value; with \"\"quote\"\"\"\n");
    check(quoted.size() == 2 && quoted[1][1] == "value; with \"quote\"", "quoted CSV fields parsed");
    std::cout << "market_core_tests: all checks passed\n";
}
