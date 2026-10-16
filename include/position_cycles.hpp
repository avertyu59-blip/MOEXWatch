#pragma once
#include "market_types.hpp"
namespace moexwatch {
bool valid_uuid(std::string_view value) noexcept;
bool transaction_before(const PortfolioTransaction& a,const PortfolioTransaction& b) noexcept;
// Requires complete persistent identity, validates IDs, order and cycle membership.
std::optional<std::vector<PositionCycle>> position_cycles(const std::vector<PortfolioTransaction>& transactions);
}
