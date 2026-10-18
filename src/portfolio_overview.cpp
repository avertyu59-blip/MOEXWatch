#include "portfolio_overview.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
namespace moexwatch {
std::optional<PortfolioOverview> calculate_portfolio_overview(
    const PortfolioSummary& portfolio, const std::vector<PositionQuoteQuality>& qualities, double cash) {
    if (!std::isfinite(cash) || cash < 0 || !std::isfinite(portfolio.total_realized_profit)) return {};
    PortfolioOverview result;
    result.manual_cash = cash;
    result.realized_profit = portfolio.total_realized_profit;
    std::map<std::string, QuoteQuality> quality;
    for (const auto& entry : qualities) {
        if (entry.quality != QuoteQuality::Fresh && entry.quality != QuoteQuality::Stale && entry.quality != QuoteQuality::Missing) return {};
        if (!quality.emplace(entry.ticker, entry.quality).second) return {};
    }
    std::map<std::string, double> ticker_values;
    for (const auto& position : portfolio.positions) {
        if (!std::isfinite(position.quantity) || position.quantity < 0 ||
            !std::isfinite(position.average_entry_price) || position.average_entry_price < 0) return {};
        if (position.quantity == 0) continue;
        result.remaining_cost += position.quantity * position.average_entry_price;
        auto state = quality.contains(position.ticker) ? quality.at(position.ticker) : QuoteQuality::Missing;
        if (position.market_value && (!std::isfinite(*position.market_value) || *position.market_value < 0)) return {};
        if (!position.market_value) state = QuoteQuality::Missing;
        if (state == QuoteQuality::Missing) { ++result.missing_count; continue; }
        const double value = *position.market_value;
        result.priced_value += value;
        ticker_values[position.ticker] += value;
        if (state == QuoteQuality::Fresh) { ++result.fresh_count; result.fresh_value += value; }
        else { ++result.stale_count; result.stale_value += value; }
    }
    if (!std::isfinite(result.remaining_cost) || !std::isfinite(result.priced_value) ||
        !std::isfinite(result.fresh_value) || !std::isfinite(result.stale_value)) return {};
    result.complete = result.missing_count == 0;
    result.all_fresh = result.complete && result.stale_count == 0;
    if (result.complete) {
        const double assets = result.priced_value + cash;
        const double profit = result.priced_value - result.remaining_cost;
        if (!std::isfinite(assets) || !std::isfinite(profit)) return {};
        result.total_assets = assets;
        result.unrealized_profit = profit;
    }
    if (result.all_fresh && result.priced_value > 0) {
        std::vector<double> values;
        for (const auto& [ticker, value] : ticker_values) { (void)ticker; values.push_back(value); }
        std::sort(values.begin(), values.end(), std::greater<double>());
        double top = 0;
        for (std::size_t i = 0; i < std::min<std::size_t>(3, values.size()); ++i) top += values[i];
        result.top3_percent = std::min(100.0, top / result.priced_value * 100.0);
    }
    return result;
}
}

namespace moexwatch {
std::optional<PortfolioIncome> calculate_portfolio_income(double realized,std::optional<double> unrealized,double received) {
    if(!std::isfinite(realized)||!std::isfinite(received)||received<0||(unrealized&&!std::isfinite(*unrealized)))return {};
    PortfolioIncome result;result.realized_with_dividends=realized+received;
    if(!std::isfinite(result.realized_with_dividends))return {};
    if(unrealized){const auto value=result.realized_with_dividends+*unrealized;if(!std::isfinite(value))return {};result.total_profit=value;}
    return result;
}
}
