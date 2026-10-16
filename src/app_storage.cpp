#include "app_storage.hpp"

#include "storage.hpp"

namespace moexwatch {

AppStorage::AppStorage(std::filesystem::path root) : root_(std::move(root)) {}

std::filesystem::path AppStorage::root() const {
    // Resolve lazily so constructing the UI owner cannot fail before its
    // normal startup error handling is available.
    return root_.empty() ? storage::data_directory() : root_;
}

SignalSettings AppStorage::load_signal_settings(SignalSettings current, bool allow_migration) const {
    storage::load_signal_settings(root(), current, allow_migration);
    return current;
}

void AppStorage::save_signal_settings(const SignalSettings& settings) const {
    storage::save_signal_settings(root(), settings);
}

std::vector<Trade> AppStorage::load_trades(std::vector<Trade> current, bool allow_migration) const {
    storage::load_trades(root(), current, allow_migration);
    return current;
}

void AppStorage::save_trades(const std::vector<Trade>& trades) const {
    storage::save_trades(root(), trades);
}

std::vector<Instrument> AppStorage::load_watchlist(std::vector<Instrument> current, bool allow_migration) const {
    storage::load_watchlist(root(), current, allow_migration);
    return current;
}

void AppStorage::save_watchlist(const std::vector<Instrument>& instruments) const {
    storage::save_watchlist(root(), instruments);
}

PortfolioJournal AppStorage::load_portfolio(bool allow_migration) const {
    return storage::load_portfolio_journal(root(), allow_migration);
}

void AppStorage::save_portfolio(const PortfolioJournal& journal) const {
    storage::save_portfolio_journal(root(), journal);
}

std::optional<std::string> AppStorage::read_text(const std::filesystem::path& path, std::size_t limit) const {
    return storage::read_text(path.is_absolute() ? path : root() / path, limit);
}

void AppStorage::atomic_write(const std::filesystem::path& path, std::string_view content) const {
    storage::atomic_write(path.is_absolute() ? path : root() / path, content);
}

std::filesystem::path AppStorage::create_backup(std::stop_token stop) const {
    return storage::create_backup(root(), stop);
}

} // namespace moexwatch
