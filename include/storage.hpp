#pragma once
#include "app_models.hpp"
#include "portfolio_identity.hpp"
#include <filesystem>
namespace moexwatch::storage {
std::filesystem::path data_directory();
PortfolioJournal load_portfolio_journal(const std::filesystem::path& root, bool allow_migration = true);
void save_portfolio_journal(const std::filesystem::path& root,const PortfolioJournal& journal);
std::optional<std::string> read_text(const std::filesystem::path& path, std::size_t limit = 4U*1024U*1024U);
void atomic_write(const std::filesystem::path& path, std::string_view content);
std::filesystem::path create_backup(const std::filesystem::path& root);
struct CandleCache { std::string ticker; std::string checked_at; std::vector<Candle> candles; };
std::optional<CandleCache> load_candles(const std::filesystem::path& root, std::string_view ticker);
void save_candles(const std::filesystem::path& root, const CandleCache& cache);
std::vector<Candle> merge_candles(const std::vector<Candle>& saved, const std::vector<Candle>& incoming, std::string_view from);
void save_signal_settings(const std::filesystem::path& root, const SignalSettings& value);
void load_signal_settings(const std::filesystem::path& root, SignalSettings& value);
void save_trades(const std::filesystem::path& root, const std::vector<Trade>& value);
void load_trades(const std::filesystem::path& root, std::vector<Trade>& value);
void save_portfolio(const std::filesystem::path& root, const std::vector<PortfolioTransaction>& value);
void load_portfolio(const std::filesystem::path& root, std::vector<PortfolioTransaction>& value);
void save_position_levels(const std::filesystem::path& root, const std::vector<PositionLevels>& value);
void load_position_levels(const std::filesystem::path& root, std::vector<PositionLevels>& value);
void save_watchlist(const std::filesystem::path& root, const std::vector<Instrument>& value);
void load_watchlist(const std::filesystem::path& root, std::vector<Instrument>& value);
}
