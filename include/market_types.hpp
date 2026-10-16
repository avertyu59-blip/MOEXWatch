#pragma once

#include <optional>
#include <cstdint>
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

enum class DividendStatus { Unknown, Forecast, Recommended, Approved, Cancelled };
enum class DividendVerification { NotChecked, Matched, Conflict, Unavailable };

struct DividendEvent {
    std::string record_date;
    double amount_per_share{};
    std::string currency;
    DividendStatus status{DividendStatus::Unknown};
    std::string period;
    std::string source_url;
    std::string reported_last_buy;
    std::string last_buy_date;
    std::string last_buy_basis;
    std::string issuer_url;
    std::string issuer_checked_at;
    DividendVerification verification{DividendVerification::NotChecked};
};

struct DividendSnapshot {
    std::string ticker;
    std::string checked_at;
    std::vector<DividendEvent> events;
};

struct TradingDay {
    std::string date;
    bool traded{};
    std::string session_date; // Weekend session can belong to a later business day.
};

struct Quote {
    std::string ticker;
    std::optional<double> last;
    std::optional<double> change_percent;
    std::string updated_at;
    std::string source_timestamp{};
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
    std::string id{};
    std::string position_id{};
    std::uint64_t sequence{};
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
    std::string position_id{};
};

struct PositionLevels {
    std::string ticker;
    std::string opened_on;
    std::optional<double> stop_loss;
    std::optional<double> take_profit;
    std::string position_id{};
};

struct PositionCycle {
    std::string position_id;
    std::string ticker;
    std::string opened_on;
    std::string closed_on;
    double quantity{};
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

enum class WatchFilter { All, Oversold, Overbought, AboveSma200, NearTarget };

} // namespace moexwatch
