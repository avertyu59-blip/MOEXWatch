#include "app_state.hpp"
#include <algorithm>
namespace moexwatch {
std::optional<std::size_t> MarketState::index_of(std::string_view ticker) const {
    const auto found=std::find_if(instruments.begin(),instruments.end(),[&](const auto& item){return item.ticker==ticker;});
    if(found==instruments.end())return {};
    return static_cast<std::size_t>(found-instruments.begin());
}
bool MarketState::add(Instrument item) {
    if(index_of(item.ticker))return false;
    instruments.push_back(std::move(item));++roster_;
    if(refreshing_)pending_=true;
    return true;
}
bool MarketState::remove(std::string_view ticker) {
    const auto index=index_of(ticker);if(!index)return false;
    instruments.erase(instruments.begin()+static_cast<std::ptrdiff_t>(*index));++roster_;
    if(refreshing_)pending_=true;
    return true;
}
std::optional<std::uint64_t> MarketState::begin_refresh() {
    if(refreshing_){pending_=true;return {};}
    refreshing_=true;requested_roster_=roster_;pending_=false;
    return ++request_;
}
MarketState::ApplyResult MarketState::apply_refresh(std::uint64_t request,MarketRefreshResult result) {
    if(!refreshing_ || request!=request_)return ApplyResult::Ignored;
    refreshing_=false;
    if(requested_roster_!=roster_){pending_=true;return ApplyResult::RosterChanged;}
    if(!result.imoex_candles.empty()) {
        imoex_candles=std::move(result.imoex_candles);imoex_stale=false;imoex_error.clear();
    } else {imoex_stale=true;imoex_error=std::move(result.imoex_error);}
    for(auto& fetched:result.instruments) {
        const auto index=index_of(fetched.ticker);if(!index)continue;
        auto& current=instruments[*index];
        // Update only market fields. Targets and dividend state belong to other actions.
        current.quote=std::move(fetched.quote);current.candles=std::move(fetched.candles);
        current.error=std::move(fetched.error);current.quote_stale=fetched.quote_stale;
        current.candles_stale=fetched.candles_stale;current.candles_checked_at=std::move(fetched.candles_checked_at);
    }
    return ApplyResult::Applied;
}
bool MarketState::take_pending_refresh() {const bool pending=pending_;pending_=false;return pending;}
bool InstrumentSelection::choose(std::string_view value,const MarketState& market) {
    if(!market.index_of(value))return false;
    ticker=std::string(value);return true;
}
std::optional<std::size_t> InstrumentSelection::index(const MarketState& market) const {return market.index_of(ticker);}
void InstrumentSelection::reconcile(const MarketState& market,const std::vector<std::size_t>& visible) {
    const auto current=index(market);
    if(current && (visible.empty() || std::find(visible.begin(),visible.end(),*current)!=visible.end()))return;
    ticker.clear();
    for(const auto row:visible)if(row<market.instruments.size()){ticker=market.instruments[row].ticker;return;}
}
}
