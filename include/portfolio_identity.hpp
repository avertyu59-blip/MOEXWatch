#pragma once
#include "position_cycles.hpp"
namespace moexwatch {
struct PortfolioJournal {
    std::vector<PortfolioTransaction> transactions;
    std::vector<PositionLevels> levels;
    std::vector<PositionLevels> unresolved_levels;
    std::uint64_t sequence_watermark{};
};
std::string new_portfolio_id();
// Assign missing identity once; preserve every existing ID or reject contradictory history.
std::vector<PortfolioTransaction> identify_portfolio(std::vector<PortfolioTransaction> transactions, std::uint64_t sequence_floor = 0);
PortfolioJournal migrate_portfolio_identity(const std::vector<PortfolioTransaction>& transactions,
    const std::vector<PositionLevels>& legacy_levels);
bool valid_portfolio_journal(const PortfolioJournal& journal);
}
