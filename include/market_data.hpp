#pragma once
#include "app_models.hpp"
#include "io_runtime.hpp"
#include <memory>
namespace moexwatch {
#ifdef _WIN32
struct FetchContext {std::stop_token stop;HttpTransport transport;std::shared_ptr<DiagnosticLog> log;std::uint64_t request{};std::filesystem::path cache_root;};
std::string local_timestamp();
void load_market_cache(std::vector<Instrument>& items, std::vector<Candle>& index, std::string& index_error);
void load_dividend_cache(Instrument& item);
void save_dividend_cache(const Instrument& item);
void fetch_dividend_events(DividendFetchResult& result,FetchContext& context);
Instrument fetch_instrument(const Instrument& previous,FetchContext& context);
std::vector<Candle> fetch_index_candles(FetchContext& context);
#endif
}
