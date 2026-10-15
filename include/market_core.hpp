#pragma once

#include <optional>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace moexwatch {

struct Candle {
    std::string date;
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
};

struct Quote {
    std::string ticker;
    std::optional<double> last;
    std::optional<double> change_percent;
    std::string updated_at;
};

struct Analysis {
    std::optional<double> sma20;
    std::optional<double> sma50;
    std::optional<double> sma200;
    std::optional<double> rsi14;
    std::optional<double> volume_ratio20;
    std::optional<double> atr14;
    std::optional<double> atr_percent;
    std::optional<double> drawdown_percent;
    std::optional<double> period_change_percent;
    std::string technical_note;
};

struct NormalizedComparison {
    std::vector<std::optional<double>> stock;
    std::vector<std::optional<double>> benchmark;
};

struct Trade {
    std::string ticker;
    double quantity{};
    double buy_price{};
    double sell_price{};
    double commission{};
    double tax_rate_percent{};
};

struct TradeResult {
    double gross_profit{};
    double profit_after_commission{};
    double tax{};
    double net_profit{};
};

enum class PortfolioSide { Buy, Sell };

struct PortfolioTransaction {
    std::string date;
    std::string ticker;
    PortfolioSide side{PortfolioSide::Buy};
    double quantity{};
    double price{};
    double commission{};
};

struct PortfolioPosition {
    std::string ticker;
    std::string opened_on;
    double quantity{};
    double average_entry_price{};
    std::optional<double> current_price;
    std::optional<double> market_value;
    std::optional<double> unrealized_profit;
    std::optional<double> unrealized_percent;
    double realized_profit{};
};

struct PositionLevels {
    std::string ticker;
    std::string opened_on;
    std::optional<double> stop_loss;
    std::optional<double> take_profit;
};

struct PortfolioSummary {
    std::vector<PortfolioPosition> positions;
    std::size_t open_positions{};
    std::size_t priced_positions{};
    double total_market_value{};
    double total_unrealized_profit{};
    double total_realized_profit{};
};

struct SignalSettings {
    bool rsi_oversold_enabled{true};
    double rsi_oversold_below{30.0};
    bool rsi_overbought_enabled{};
    double rsi_overbought_above{70.0};
    bool above_sma200_enabled{true};
    bool near_target_enabled{true};
    double near_target_percent{5.0};
    bool require_all{};
    bool notifications_enabled{true};
    bool auto_refresh_enabled{true};
    int auto_refresh_minutes{15};
};

struct SignalEvaluation {
    bool matched{};
    std::vector<std::string> reasons;
};

struct BacktestSummary {
    double starting_capital{};
    double strategy_final_value{};
    double benchmark_final_value{};
    double strategy_return_percent{};
    double benchmark_return_percent{};
    double strategy_max_drawdown_percent{};
    std::size_t completed_trades{};
    std::string start_date;
    std::string end_date;
};

std::vector<std::vector<std::string>> parse_csv(std::string_view csv, char delimiter = ';');
std::optional<Quote> parse_quote_csv(std::string_view csv, std::string_view ticker);
std::vector<Candle> parse_candles_csv(std::string_view csv);
bool has_candle_header(std::string_view csv);
Analysis analyze(const std::vector<Candle>& candles);
std::vector<std::optional<double>> sma_series(const std::vector<Candle>& candles, std::size_t period);
std::vector<std::optional<double>> rsi_series(const std::vector<Candle>& candles, std::size_t period = 14);
std::vector<std::optional<double>> atr_series(const std::vector<Candle>& candles, std::size_t period = 14);
std::vector<std::optional<double>> volume_ratio_series(const std::vector<Candle>& candles, std::size_t period = 20);
NormalizedComparison normalized_comparison_series(const std::vector<Candle>& stock,
                                                   const std::vector<Candle>& benchmark,
                                                   std::size_t start_index = 0);
enum class WatchFilter { All, Oversold, Overbought, AboveSma200, NearTarget };
bool matches_filter(const Analysis& analysis, std::optional<double> last, std::optional<double> target,
                    bool stale, WatchFilter filter);
bool valid_ticker(std::string_view ticker);
std::string format_price(double value);
bool valid_trade(const Trade& trade) noexcept;
std::optional<TradeResult> calculate_trade(const Trade& trade) noexcept;
bool valid_portfolio_transaction(const PortfolioTransaction& transaction) noexcept;
bool valid_position_levels(const PositionLevels& levels) noexcept;
std::optional<PortfolioSummary> calculate_portfolio(const std::vector<PortfolioTransaction>& transactions,
                                                    const std::vector<Quote>& quotes);
bool valid_signal_settings(const SignalSettings& settings) noexcept;
SignalEvaluation evaluate_signals(const Analysis& analysis, std::optional<double> last,
                                  std::optional<double> target, bool quote_stale,
                                  bool candles_stale, const SignalSettings& settings);
std::optional<BacktestSummary> run_backtest(const std::vector<Candle>& stock,
                                            const std::vector<Candle>& index,
                                            const SignalSettings& settings,
                                            std::optional<double> target,
                                            double starting_capital,
                                            double commission_percent_per_side);

} // namespace moexwatch
