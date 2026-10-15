#pragma once
#include "market_core.hpp"
namespace moexwatch {
enum class ImportStatus { New, Duplicate, Error };
struct ImportRow {
    std::size_t line{};
    std::optional<PortfolioTransaction> transaction;
    ImportStatus status{ImportStatus::Error};
    std::string message;
};
struct ImportPreview {
    std::vector<ImportRow> rows;
    std::string error;
};
ImportPreview preview_portfolio_csv(std::string_view csv, const std::vector<PortfolioTransaction>& existing);
std::optional<std::vector<PortfolioTransaction>> prepare_portfolio_import(const ImportPreview& preview,
    const std::vector<PortfolioTransaction>& existing, bool include_duplicates, std::string& error);
std::string export_portfolio_csv(const std::vector<PortfolioTransaction>& transactions);
std::optional<double> portfolio_weight(const PortfolioSummary& summary, const PortfolioPosition& position, bool all_prices_fresh);
} // namespace moexwatch
