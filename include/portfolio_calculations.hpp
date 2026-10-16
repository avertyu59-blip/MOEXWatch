#pragma once
#include "market_types.hpp"
namespace moexwatch {
std::optional<PortfolioSummary> calculate_portfolio(const std::vector<PortfolioTransaction>& transactions,
                                                    const std::vector<Quote>& quotes);
bool valid_trade(const Trade& trade) noexcept;
std::optional<TradeResult> calculate_trade(const Trade& trade) noexcept;
bool valid_portfolio_transaction(const PortfolioTransaction& transaction) noexcept;
bool valid_position_levels(const PositionLevels& levels) noexcept;
std::optional<double> portfolio_weight(const PortfolioSummary&, const PortfolioPosition&, bool all_prices_fresh);
std::optional<TradeResult> calculate_trade_totals(const std::vector<Trade>& trades);
}
