#pragma once
#include "app_models.hpp"
namespace moexwatch {
#ifdef _WIN32
std::string local_timestamp();
void load_market_cache(std::vector<Instrument>& items, std::vector<Candle>& index, std::string& index_error);
void load_dividend_cache(Instrument& item);
void save_dividend_cache(const Instrument& item);
void fetch_dividend_events(DividendFetchResult& result);
Instrument fetch_instrument(const Instrument& previous);
std::vector<Candle> fetch_index_candles();
#endif
}
