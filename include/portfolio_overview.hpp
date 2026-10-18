#pragma once
#include "market_types.hpp"
namespace moexwatch {
enum class QuoteQuality { Fresh, Stale, Missing };
struct PositionQuoteQuality { std::string ticker; QuoteQuality quality{QuoteQuality::Missing}; };
struct PortfolioOverview {
    double remaining_cost{}, manual_cash{}, priced_value{}, fresh_value{}, stale_value{}, realized_profit{};
    std::size_t fresh_count{}, stale_count{}, missing_count{};
    bool complete{}, all_fresh{};
    std::optional<double> total_assets, unrealized_profit, top3_percent;
};
// Missing prices never produce an apparently complete valuation. Cash is an independent manual balance.
std::optional<PortfolioOverview> calculate_portfolio_overview(
    const PortfolioSummary&, const std::vector<PositionQuoteQuality>&, double manual_cash);
struct PortfolioIncome { double realized_with_dividends{}; std::optional<double> total_profit; };
std::optional<PortfolioIncome> calculate_portfolio_income(double trading_realized, std::optional<double> unrealized, double received_dividend_net);
}
