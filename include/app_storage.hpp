#pragma once

#include "app_models.hpp"
#include "portfolio_identity.hpp"

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace moexwatch {

// Application-facing persistence boundary. It translates typed application
// models to the existing storage format and owns the user-data directory.
// It has no dependency on Win32 or UI error reporting.
class AppStorage {
public:
    explicit AppStorage(std::filesystem::path root = {});

    std::filesystem::path root() const;

    SignalSettings load_signal_settings(SignalSettings current = {}, bool allow_migration = true) const;
    void save_signal_settings(const SignalSettings& settings) const;

    std::vector<Trade> load_trades(std::vector<Trade> current = {}, bool allow_migration = true) const;
    void save_trades(const std::vector<Trade>& trades) const;

    std::vector<Instrument> load_watchlist(std::vector<Instrument> current = {}, bool allow_migration = true) const;
    void save_watchlist(const std::vector<Instrument>& instruments) const;

    PortfolioJournal load_portfolio(bool allow_migration = true) const;
    void save_portfolio(const PortfolioJournal& journal) const;

    std::optional<std::string> read_text(const std::filesystem::path& path, std::size_t limit = 4U * 1024U * 1024U) const;
    void atomic_write(const std::filesystem::path& path, std::string_view content) const;
    std::filesystem::path create_backup(std::stop_token stop = {}) const;

private:
    std::filesystem::path root_;
};

} // namespace moexwatch
