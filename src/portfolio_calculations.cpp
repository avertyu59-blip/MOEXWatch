#include "portfolio_calculations.hpp"
#include "domain_common.hpp"

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
std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

}
bool valid_trade(const Trade& trade) noexcept {
    return valid_ticker(trade.ticker) && std::isfinite(trade.quantity) && trade.quantity > 0.0 &&
           std::isfinite(trade.buy_price) && trade.buy_price > 0.0 &&
           std::isfinite(trade.sell_price) && trade.sell_price > 0.0 &&
           std::isfinite(trade.commission) && trade.commission >= 0.0 &&
           std::isfinite(trade.tax_rate_percent) && trade.tax_rate_percent >= 0.0 &&
           trade.tax_rate_percent <= 100.0;
}

std::optional<TradeResult> calculate_trade(const Trade& trade) noexcept {
    if (!valid_trade(trade)) return std::nullopt;
    TradeResult result;
    result.gross_profit = (trade.sell_price - trade.buy_price) * trade.quantity;
    result.profit_after_commission = result.gross_profit - trade.commission;
    const double taxable_profit = std::max(0.0, result.profit_after_commission);
    result.tax = taxable_profit * trade.tax_rate_percent / 100.0;
    result.net_profit = result.profit_after_commission - result.tax;
    if (!std::isfinite(result.gross_profit) || !std::isfinite(result.profit_after_commission) ||
        !std::isfinite(result.tax) || !std::isfinite(result.net_profit)) return std::nullopt;
    return result;
}


namespace {
bool valid_iso_date(std::string_view date) noexcept {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    const auto parse_part = [&](std::size_t begin, std::size_t count) -> int {
        int value{};
        const auto* first = date.data() + begin;
        const auto* last = first + count;
        const auto [end, error] = std::from_chars(first, last, value);
        return error == std::errc{} && end == last ? value : -1;
    };
    const int y = parse_part(0, 4), m = parse_part(5, 2), d = parse_part(8, 2);
    return y >= 1900 && std::chrono::year_month_day{std::chrono::year{y},
        std::chrono::month{static_cast<unsigned>(m)}, std::chrono::day{static_cast<unsigned>(d)}}.ok();
}
}

bool valid_portfolio_transaction(const PortfolioTransaction& transaction) noexcept {
    return valid_iso_date(transaction.date) && valid_ticker(transaction.ticker) &&
           std::isfinite(transaction.quantity) && transaction.quantity > 0.0 &&
           std::isfinite(transaction.price) && transaction.price > 0.0 &&
           std::isfinite(transaction.commission) && transaction.commission >= 0.0 &&
           (transaction.side == PortfolioSide::Buy || transaction.side == PortfolioSide::Sell);
}

bool valid_position_levels(const PositionLevels& levels) noexcept {
    const auto valid_level = [](const std::optional<double>& value) {
        return !value || (std::isfinite(*value) && *value > 0.0);
    };
    return valid_ticker(levels.ticker) && valid_iso_date(levels.opened_on) &&
           valid_level(levels.stop_loss) && valid_level(levels.take_profit);
}

std::optional<PortfolioSummary> calculate_portfolio(const std::vector<PortfolioTransaction>& transactions,
                                                    const std::vector<Quote>& quotes) {
    struct State { double quantity{}; double average_cost{}; double realized{}; std::string opened_on; };
    std::vector<PortfolioTransaction> ordered = transactions;
    for (const auto& transaction : ordered) if (!valid_portfolio_transaction(transaction)) return std::nullopt;
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        return left.date < right.date;
    });
    std::map<std::string, State> states;
    for (const auto& transaction : ordered) {
        auto& state = states[upper(transaction.ticker)];
        if (transaction.side == PortfolioSide::Buy) {
            if (state.quantity == 0.0) state.opened_on = transaction.date;
            const double old_cost = state.quantity * state.average_cost;
            const double added_cost = transaction.quantity * transaction.price + transaction.commission;
            const double new_quantity = state.quantity + transaction.quantity;
            const double new_cost = old_cost + added_cost;
            if (!std::isfinite(new_quantity) || !std::isfinite(new_cost) || new_quantity <= 0.0) return std::nullopt;
            state.quantity = new_quantity;
            state.average_cost = new_cost / new_quantity;
        } else {
            constexpr double quantity_epsilon = 1e-9;
            if (transaction.quantity > state.quantity + quantity_epsilon) return std::nullopt;
            state.realized += transaction.quantity * (transaction.price - state.average_cost) - transaction.commission;
            state.quantity -= transaction.quantity;
            if (state.quantity <= quantity_epsilon) { state.quantity = 0.0; state.average_cost = 0.0; state.opened_on.clear(); }
            if (!std::isfinite(state.realized)) return std::nullopt;
        }
    }
    std::map<std::string, std::optional<double>> last_prices;
    for (const auto& quote : quotes) {
        if (valid_ticker(quote.ticker)) last_prices[upper(quote.ticker)] =
            quote.last && std::isfinite(*quote.last) && *quote.last > 0.0 ? quote.last : std::nullopt;
    }
    PortfolioSummary result;
    for (const auto& [ticker, state] : states) {
        PortfolioPosition position;
        position.ticker = ticker;
        position.opened_on = state.opened_on;
        position.quantity = state.quantity;
        position.average_entry_price = state.average_cost;
        position.realized_profit = state.realized;
        result.total_realized_profit += state.realized;
        if (state.quantity > 0.0) {
            ++result.open_positions;
            const auto price = last_prices.find(ticker);
            if (price != last_prices.end() && price->second) {
                position.current_price = price->second;
                position.market_value = state.quantity * *price->second;
                position.unrealized_profit = *position.market_value - state.quantity * state.average_cost;
                const double basis = state.quantity * state.average_cost;
                if (basis > 0.0) position.unrealized_percent = *position.unrealized_profit / basis * 100.0;
                result.total_market_value += *position.market_value;
                result.total_unrealized_profit += *position.unrealized_profit;
                ++result.priced_positions;
            }
        }
        result.positions.push_back(std::move(position));
    }
    if (!std::isfinite(result.total_realized_profit) || !std::isfinite(result.total_market_value) ||
        !std::isfinite(result.total_unrealized_profit)) return std::nullopt;
    return result;
}


std::optional<double> portfolio_weight(const PortfolioSummary& summary,const PortfolioPosition& position,bool fresh){
 if(!fresh||summary.open_positions==0||summary.priced_positions!=summary.open_positions||summary.total_market_value<=0||!std::isfinite(summary.total_market_value)||!position.market_value||position.quantity<=0)return {};
 const double value=*position.market_value/summary.total_market_value*100.0;
 return std::isfinite(value)?std::optional<double>(value):std::nullopt;
}

std::optional<TradeResult> calculate_trade_totals(const std::vector<Trade>& trades) {
    TradeResult total;
    for (const auto& trade : trades) {
        const auto value = calculate_trade(trade);
        if (!value) return {};
        total.gross_profit += value->gross_profit;
        total.profit_after_commission += value->profit_after_commission;
        total.tax += value->tax;
        total.net_profit += value->net_profit;
    }
    if (!std::isfinite(total.gross_profit) || !std::isfinite(total.profit_after_commission) ||
        !std::isfinite(total.tax) || !std::isfinite(total.net_profit)) return {};
    return total;
}

} // namespace moexwatch
