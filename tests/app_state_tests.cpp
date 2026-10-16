#include "app_state.hpp"
#include <iostream>
#include <stdexcept>
using namespace moexwatch;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
Instrument item(std::string ticker,double price){Instrument result;result.ticker=ticker;result.quote.ticker=ticker;result.quote.last=price;result.quote_stale=false;return result;}
}
int main(){try{
    MarketState market;
    check(market.add(item("SBER",100)),"first stock added");
    check(market.add(item("GAZP",90)),"second stock added");
    check(!market.add(item("SBER",200)),"duplicate stock rejected");
    InstrumentSelection selection;
    check(selection.choose("GAZP",market),"choose by ticker");
    check(!selection.choose("UNKNOWN",market)&&selection.ticker=="GAZP","invalid selection preserves previous stock");
    market.remove("SBER");
    check(selection.index(market)==0,"selection survives removing an earlier row");
    selection.reconcile(market,{});
    check(selection.ticker=="GAZP","empty filter preserves selection identity");
    market.add(item("SBER",100));selection.reconcile(market,{1});
    check(selection.ticker=="SBER","hidden selection falls back to a visible stock");
    market.instruments[1].target=150;
    DividendEvent event;event.amount_per_share=10;market.instruments[1].dividends={event};
    const auto first=market.begin_refresh();check(first.has_value(),"request starts");
    market.instruments[1].target=155; // Edit while request is in flight.
    check(!market.begin_refresh(),"overlapping request queued without another worker");
    auto fetched=item("SBER",120);fetched.target=999;fetched.dividends={};
    MarketRefreshResult response;response.instruments={fetched};response.imoex_candles={{"2026-01-01",1,1,1,1,1}};
    check(market.apply_refresh(*first+1,response)==MarketState::ApplyResult::Ignored&&market.refreshing(),"wrong response cannot finish active request");
    check(market.apply_refresh(*first,response)==MarketState::ApplyResult::Applied,"matching response applied");
    check(market.instruments[1].quote.last==120&&market.instruments[1].target==155,"market update preserves edited target");
    check(market.instruments[1].dividends.size()==1,"market update preserves independently loaded dividends");
    check(!market.imoex_stale,"benchmark success clears stale flag");
    check(market.take_pending_refresh()&&!market.take_pending_refresh(),"pending refresh consumed exactly once");
    const auto second=market.begin_refresh();check(second.has_value(),"next request starts");
    check(market.apply_refresh(*first,response)==MarketState::ApplyResult::Ignored&&market.refreshing(),"late older response cannot finish newer request");
    market.remove("SBER");market.add(item("SBER",77));
    check(market.apply_refresh(*second,response)==MarketState::ApplyResult::RosterChanged,"remove and re-add invalidates snapshot");
    check(market.instruments[1].quote.last==77,"old quote cannot attach to re-added stock");
    check(market.take_pending_refresh(),"roster change requests fresh snapshot");
    const auto third=market.begin_refresh();check(third.has_value(),"replacement request starts");
    MarketRefreshResult failed;failed.imoex_error="offline";
    check(market.apply_refresh(*third,failed)==MarketState::ApplyResult::Applied,"failure response completes active request");
    check(market.imoex_stale&&!market.imoex_candles.empty()&&market.imoex_error=="offline","benchmark failure preserves old data explicitly stale");
    check(market.apply_refresh(*third,response)==MarketState::ApplyResult::Ignored,"duplicate completion ignored");
    market.remove("SBER");selection.reconcile(market,{});check(selection.ticker.empty(),"deleted selection is cleared");
    MarketState independent;independent.add(item("SBER",555));
    check(market.instruments[0].quote.last==90&&independent.instruments[0].quote.last==555,"application instances do not share state");
    std::cout<<"app_state_tests: all checks passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
