#include "market_data_service.hpp"
#include "market_data.hpp"
#include "storage.hpp"
#include <QDate>
#include <algorithm>
#include <cmath>
using namespace moexwatch;
namespace {
bool validHistory(const std::vector<Candle> &candles) {
    std::string previous;
    for (const auto &c : candles) {
        const auto date = c.date.substr(0, 10);
        const auto q = QString::fromStdString(date);
        if (QDate::fromString(q, Qt::ISODate).toString(Qt::ISODate) != q ||
            (!previous.empty() && date <= previous))
            return false;
        if (!std::isfinite(c.open) || !std::isfinite(c.high) || !std::isfinite(c.low) ||
            !std::isfinite(c.close) || !std::isfinite(c.volume) || c.open <= 0 || c.close <= 0 ||
            c.low <= 0 || c.high < std::max(c.open, c.close) || c.low > std::min(c.open, c.close) ||
            c.volume < 0)
            return false;
        previous = date;
    }
    return true;
}
} // namespace
void MarketDataService::load(const AppStorage &storage) { items_ = storage.load_watchlist(); }
void MarketDataService::loadCaches(const AppStorage &storage) {
    for (auto &item : items_)
        try {
            if (const auto cache = storage::load_candles(storage.root(), item.ticker)) {
                item.candles = cache->candles;
                item.candles_checked_at = cache->checked_at;
            }
        } catch (const std::exception &ex) {
            item.error = ex.what();
        }
    try {
        if (const auto cache = storage::load_candles(storage.root(), "IMOEX"))
            index_ = cache->candles;
    } catch (const std::exception &ex) {
        index_error_ = ex.what();
    }
}
MarketDataService::Fetch MarketDataService::source(std::filesystem::path root) {
    return [root = std::move(root)](std::vector<Instrument> items, std::stop_token stop) {
        MarketRefreshResult result;
#ifdef _WIN32
        FetchContext context{stop, winhttp_get,
                             std::make_shared<DiagnosticLog>(root / "logs" / "diagnostic.log"), 0, root};
        try {
            result.imoex_candles = fetch_index_candles(context);
        } catch (const Cancelled &) {
            throw;
        } catch (const std::exception &ex) {
            result.imoex_error = ex.what();
        }
        for (const auto &item : items) {
            check_cancelled(stop);
            result.instruments.push_back(fetch_instrument(item, context));
        }
#else
        (void)root;
        (void)items;
        (void)stop;
        throw std::runtime_error("Сетевой адаптер доступен в Windows-сборке");
#endif
        return result;
    };
}
void MarketDataService::apply(MarketRefreshResult result) {
    for (auto &incoming : result.instruments) {
        const auto i = std::find_if(items_.begin(), items_.end(),
                                    [&](const auto &old) { return old.ticker == incoming.ticker; });
        if (i == items_.end())
            continue; // Never resurrect a deleted watchlist entry.
        if (incoming.quote.ticker == i->ticker &&
            (!incoming.quote.last || (std::isfinite(*incoming.quote.last) && *incoming.quote.last > 0))) {
            i->quote = std::move(incoming.quote);
            i->quote_stale = incoming.quote_stale;
        } else {
            i->quote_stale = true;
            incoming.error += "; Некорректная котировка отклонена";
        }
        if (!incoming.candles.empty() && validHistory(incoming.candles)) {
            i->candles = std::move(incoming.candles);
            i->candles_stale = incoming.candles_stale;
            i->candles_checked_at = std::move(incoming.candles_checked_at);
        } else {
            i->candles_stale = true;
            incoming.error += "; Некорректная история отклонена";
        }
        i->error = std::move(incoming.error); // Keep locally edited target/dividend fields.
    }
    if (result.imoex_error.empty() && !result.imoex_candles.empty() && validHistory(result.imoex_candles)) {
        index_ = std::move(result.imoex_candles);
        index_stale_ = false;
        index_error_.clear();
    } else {
        index_stale_ = true;
        index_error_ =
            result.imoex_error.empty() ? "Нет корректной истории IMOEX" : std::move(result.imoex_error);
    }
}
