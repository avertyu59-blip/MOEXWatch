#pragma once
#include "app_models.hpp"
#include <cstdint>
namespace moexwatch {
// Owned and mutated by the UI thread; workers receive copies and return messages.
class MarketState {
    std::uint64_t request_{};
    std::uint64_t roster_{};
    std::uint64_t requested_roster_{};
    bool refreshing_{};
    bool pending_{};
public:
    std::vector<Instrument> instruments;
    std::vector<Candle> imoex_candles;
    bool imoex_stale{true};
    std::string imoex_error;
    std::optional<std::size_t> index_of(std::string_view ticker) const;
    bool add(Instrument item);
    bool remove(std::string_view ticker);
    std::optional<std::uint64_t> begin_refresh();
    enum class ApplyResult { Ignored, Applied, RosterChanged };
    ApplyResult apply_refresh(std::uint64_t request, MarketRefreshResult result);
    bool refreshing() const { return refreshing_; }
    bool take_pending_refresh();
};
struct InstrumentSelection {
    std::string ticker;
    bool choose(std::string_view value, const MarketState& market);
    std::optional<std::size_t> index(const MarketState& market) const;
    void reconcile(const MarketState& market, const std::vector<std::size_t>& visible);
};
struct PersistenceState { bool writes_blocked{}; };
struct ApplicationState {
    MarketState market;
    PersistenceState persistence;
};
}
