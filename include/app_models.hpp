#pragma once
#include "market_core.hpp"
namespace moexwatch {
struct Instrument {
    std::string ticker;
    std::optional<double> target;
    Quote quote;
    std::vector<Candle> candles;
    std::string error;
    bool quote_stale{true};
    bool candles_stale{true};
    std::string candles_checked_at;
    std::vector<DividendEvent> dividends;
    std::string dividend_error;
    std::string dividend_note;
    std::string dividends_checked_at;
    bool dividends_stale{true};
};

struct DividendFetchResult {
    std::string ticker;
    std::vector<DividendEvent> events;
    std::string error;
    std::string checked_at;
    std::string note;
};

struct MarketRefreshResult {
    std::vector<Instrument> instruments;
    std::vector<Candle> imoex_candles;
    std::string imoex_error;
};

}
