#include "storage.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace moexwatch;
namespace st=moexwatch::storage;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> bool fails(F&& f){try{f();return false;}catch(const std::exception&){return true;}}
struct Temp {
 std::filesystem::path root=std::filesystem::temp_directory_path()/("moexwatch-storage-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Temp(){std::filesystem::create_directories(root);}
 ~Temp(){std::error_code ignored;std::filesystem::remove_all(root,ignored);}
};
}
int main(){
 try {
 Temp tmp;const auto root=tmp.root;
 check(!st::read_text(root/"missing.csv"),"missing file distinguished from unreadable file");
 st::atomic_write(root/"watchlist.csv","# ticker;target\nSBER;250\nGAZP;\n");
 std::vector<Instrument> instruments;st::load_watchlist(root,instruments);
 check(instruments.size()==2 && instruments[0].target==250,"legacy watchlist preserved");
 st::save_watchlist(root,instruments);std::vector<Instrument> again;st::load_watchlist(root,again);
 check(again.size()==2 && !again[1].target,"watchlist round trip");
 std::vector<Trade> trades{{"SBER",2,100,120,3,13}};st::save_trades(root,trades);
 std::vector<Trade> loaded;st::load_trades(root,loaded);check(loaded.size()==1 && calculate_trade(loaded[0])->net_profit==calculate_trade(trades[0])->net_profit,"closed-trade math unchanged after persistence refactor");
 std::vector<PortfolioTransaction> portfolio{{"2026-10-01","SBER",PortfolioSide::Buy,3,100,2},{"2026-10-02","SBER",PortfolioSide::Sell,1,110,1}};
 st::save_portfolio(root,portfolio);std::vector<PortfolioTransaction> pl;st::load_portfolio(root,pl);
 check(pl.size()==2 && calculate_portfolio(pl,{})->positions[0].quantity==2,"portfolio side and chronological order survive round trip");
 std::vector<PositionLevels> levels{{"SBER","2026-10-01",90,130}};st::save_position_levels(root,levels);
 std::vector<PositionLevels> ll;st::load_position_levels(root,ll);check(ll.size()==1 && ll[0].opened_on==levels[0].opened_on && ll[0].stop_loss==90,"levels remain attached to opening cycle");
 SignalSettings signals;signals.require_all=true;signals.auto_refresh_minutes=30;signals.rsi_oversold_below=25;
 st::save_signal_settings(root,signals);SignalSettings sl;st::load_signal_settings(root,sl);
 check(sl.require_all && sl.auto_refresh_minutes==30 && sl.rsi_oversold_below==25,"signal settings legacy format preserved");
 st::atomic_write(root/"dividends_SBER.csv",serialize_dividend_snapshot({"SBER","2026-10-15",{}}));
 const auto backup=st::create_backup(root);
 for(const auto* name:{"watchlist.csv","trades.csv","portfolio_transactions.csv","position_levels.csv","signals.ini","dividends_SBER.csv"})check(st::read_text(root/name)==st::read_text(backup/name),"backup bytes verified");
 check(st::read_text(backup/"RESTORE.txt").has_value(),"backup includes restoration instructions");
 st::atomic_write(root/"watchlist.csv","# ticker;target\nLKOH;5000\n");
 check(st::read_text(backup/"watchlist.csv")!=st::read_text(root/"watchlist.csv"),"existing backup never modified by later saves");
 const auto restored_root=root/"restored";
 std::filesystem::create_directories(restored_root);
 for(const auto& file:std::filesystem::directory_iterator(backup))if(file.is_regular_file())std::filesystem::copy_file(file.path(),restored_root/file.path().filename());
 std::vector<PortfolioTransaction> restored_portfolio;st::load_portfolio(restored_root,restored_portfolio);
 check(calculate_portfolio(restored_portfolio,{})->total_realized_profit==calculate_portfolio(portfolio,{})->total_realized_profit,"restored backup reproduces realized portfolio profit");
 const auto unicode_root=root/std::filesystem::path(u8"данные");
 st::save_watchlist(unicode_root,instruments);std::vector<Instrument> unicode_loaded;st::load_watchlist(unicode_root,unicode_loaded);
 check(unicode_loaded.size()==instruments.size(),"Unicode Windows data path supported");
 const auto backup2=st::create_backup(root);check(backup!=backup2,"each backup has distinct identity");
 st::atomic_write(root/"trades.csv","corrupt\n");
 check(fails([&]{std::vector<Trade> out;st::load_trades(root,out);}),"invalid user journal reported, not silently treated as empty");
 check(*st::read_text(root/"trades.csv")=="corrupt\n","failed load does not touch damaged original");
 std::filesystem::create_directory(root/"blocked");
 check(fails([&]{st::atomic_write(root/"blocked","replacement");}),"failed atomic replacement reported");
 check(std::filesystem::is_directory(root/"blocked") && !std::filesystem::exists(root/"blocked.tmp"),"failed replacement preserves destination and removes temporary file");
 check(fails([&]{st::read_text(root/"watchlist.csv",2);}),"bounded reads reject oversized input");
 const std::vector<Candle> bars{{"2026-10-01",100,105,98,102,1000},{"2026-10-02",102,107,100,104,1200}};
 st::save_candles(root,{"SBER","2026-10-15 21:00",bars});
 const auto cache=st::load_candles(root,"SBER");
 check(cache && cache->candles.size()==2 && cache->checked_at=="2026-10-15 21:00" && cache->candles.back().close==104,"cache carries full candles and observation time");
 const auto merged=st::merge_candles(bars,{{"2026-10-02",102,108,100,106,1400},{"2026-10-05",106,110,104,109,1600}},"2026-10-02");
 check(merged.size()==2 && merged[0].close==106 && merged[0].volume==1400 && merged[1].date=="2026-10-05","incremental update replaces incomplete last candle and trims old window");
 check(fails([&]{st::save_candles(root,{"../escape","now",bars});}),"ticker cannot escape cache directory");
 auto bad=bars;bad[1].close=200;
 check(fails([&]{st::save_candles(root,{"SBER","now",bad});}),"invalid OHLC not committed to cache");
 check(st::load_candles(root,"SBER")->candles.back().close==104,"invalid new history preserves previous complete cache");
 bad=bars;bad[1].date=bad[0].date;
 check(fails([&]{st::save_candles(root,{"SBER","now",bad});}),"duplicate daily candles rejected");
 const auto cachefile=root/"cache"/"SBER-TQBR-D1.csv";
 auto bytes=*st::read_text(cachefile);bytes.erase(bytes.find_last_of('\n',bytes.size()-2)+1);
 st::atomic_write(cachefile,bytes);
 check(fails([&]{st::load_candles(root,"SBER");}),"truncated candle cache cannot be accepted as complete");
 check(!st::load_candles(root,"IMOEX"),"stock and index caches separated");
 const auto backup3=st::create_backup(root);check(!std::filesystem::exists(backup3/"cache"),"disposable market cache excluded from personal backup");
 // Concurrent file replacement and backup use the same lock; no mixed bytes.
 st::save_trades(root,trades);
 std::thread writer([&]{for(int i=0;i<8;++i)st::save_trades(root,trades);});
 const auto concurrent=st::create_backup(root);writer.join();
 check(st::read_text(concurrent/"trades.csv")==st::read_text(root/"trades.csv"),"backup and saves serialized without partial records");
 std::cout<<"storage_tests: all checks passed\n";
 } catch(const std::exception& ex) {std::cerr<<"storage_tests: "<<ex.what()<<std::endl;return 1;}
}
