#pragma once
#include "market_core.hpp"
namespace moexwatch {
enum class ImportStatus { New, Duplicate, Error };
struct ImportRow {
    std::size_t line{};
    std::optional<PortfolioTransaction> transaction;
    ImportStatus status{ImportStatus::Error};
    std::string message;
    bool identity_duplicate{};
};
struct ImportPreview {
    std::vector<ImportRow> rows;
    std::string error;
};
ImportPreview preview_portfolio_csv(std::string_view csv, const std::vector<PortfolioTransaction>& existing);
std::optional<std::vector<PortfolioTransaction>> prepare_portfolio_import(const ImportPreview& preview,
    const std::vector<PortfolioTransaction>& existing, bool include_duplicates, std::string& error, std::uint64_t sequence_floor = 0);
std::string export_portfolio_csv(const std::vector<PortfolioTransaction>& transactions);
} // namespace moexwatch
