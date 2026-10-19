#pragma once
#include "app_storage.hpp"
#include <functional>
class MarketDataService {
  public:
    using Fetch =
        std::function<moexwatch::MarketRefreshResult(std::vector<moexwatch::Instrument>, std::stop_token)>;
    void load(const moexwatch::AppStorage &storage);
    void loadCaches(const moexwatch::AppStorage &storage);
    static Fetch source(std::filesystem::path root);
    void apply(moexwatch::MarketRefreshResult result);
    void replace(std::vector<moexwatch::Instrument> items) { items_ = std::move(items); }
    const std::vector<moexwatch::Instrument> &instruments() const { return items_; }
    // Only the UI-thread coordinator can edit targets and the active watchlist.
    std::vector<moexwatch::Instrument> &instruments() { return items_; }
    const std::vector<moexwatch::Candle> &index() const { return index_; }
    bool indexStale() const { return index_stale_; }
    const std::string &indexError() const { return index_error_; }
    void demoIndex(std::vector<moexwatch::Candle> candles) {
        index_ = std::move(candles);
        index_stale_ = false;
        index_error_.clear();
    }

  private:
    std::vector<moexwatch::Instrument> items_;
    std::vector<moexwatch::Candle> index_;
    bool index_stale_{true};
    std::string index_error_;
};
