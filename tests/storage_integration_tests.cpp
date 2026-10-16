#include "storage.hpp"
#include "portfolio_csv.hpp"
#include "observation.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace moexwatch;
namespace st=moexwatch::storage;
namespace {
void check(bool b,const char* reason){if(!b)throw std::runtime_error(reason);}
template<class F>void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("expected failure");}
struct Temp {std::filesystem::path root=std::filesystem::temp_directory_path()/("moex-integration-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));Temp(){std::filesystem::create_directories(root);}~Temp(){std::error_code e;std::filesystem::remove_all(root,e);}};
}
int main(){try{
    Temp temp;const auto root=temp.root;
    const std::string old_watch="# ticker;target\nSBER;250\nGAZP;\n";
    st::atomic_write(root/"watchlist.csv",old_watch);
    std::vector<Instrument> watch;st::load_watchlist(root,watch,false);
    check(watch.size()==2&&*st::read_text(root/"watchlist.csv")==old_watch,"read-only load never migrates");
    st::load_watchlist(root,watch);const auto versioned=*st::read_text(root/"watchlist.csv");
    check(versioned.starts_with("MOEXWATCH_WATCHLIST;1;"),"legacy watchlist migrated");
    bool found=false;for(const auto& entry:std::filesystem::recursive_directory_iterator(root/"backups"))if(entry.path().filename()=="watchlist.csv"&&st::read_text(entry.path())==old_watch)found=true;
    check(found,"original bytes backed up before migration");
    std::vector<Instrument> restart;st::load_watchlist(root,restart);check(restart.size()==watch.size()&&restart[0].target==250,"watchlist restart preserves values");
    std::size_t backups=0;for(const auto& e:std::filesystem::directory_iterator(root/"backups")){(void)e;++backups;}st::load_watchlist(root,restart);
    std::size_t backups_after=0;for(const auto& e:std::filesystem::directory_iterator(root/"backups")){(void)e;++backups_after;}check(backups==backups_after,"migration occurs only once");
    for(auto damaged:{versioned.substr(0,versioned.size()-2),std::string("MOEXWATCH_WATCHLIST;99;0;00000000\n"),std::string("# ticker;target\nSBER;250\nGAZP;bad\n"),std::string("# ticker;target\nSBER;250\nSBER;260\n")}){
        st::atomic_write(root/"watchlist.csv",damaged);const auto before=restart.size();
        rejects([&]{st::load_watchlist(root,restart);});check(restart.size()==before&&restart[0].target==250&&st::read_text(root/"watchlist.csv")==damaged,"failed load preserves model and original file");
        if(damaged.starts_with("MOEXWATCH_"))rejects([&]{st::save_watchlist(root,watch);});
    }
    auto corrupt=versioned;corrupt.back()='X';st::atomic_write(root/"watchlist.csv",corrupt);rejects([&]{st::load_watchlist(root,restart);});
    st::atomic_write(root/"watchlist.csv",versioned);
    st::atomic_write(root/"trades.csv","ticker;quantity;buy_price;sell_price;commission;tax_rate_percent\nSBER;10;100;120;2;13\n");
    std::vector<Trade> trades;st::load_trades(root,trades);check(trades.size()==1&&st::read_text(root/"trades.csv")->starts_with("MOEXWATCH_TRADES;1;"),"closed trades migrate");
    const auto net=calculate_trade_totals(trades)->net_profit;std::vector<Trade> trades_restart;st::load_trades(root,trades_restart);check(calculate_trade_totals(trades_restart)->net_profit==net,"net profit survives restart");
    const auto trades_bytes=*st::read_text(root/"trades.csv");st::atomic_write(root/"trades.csv",trades_bytes+"\n");rejects([&]{st::load_trades(root,trades_restart);});check(trades_restart.size()==1,"trailing corruption does not append partial records");st::atomic_write(root/"trades.csv",trades_bytes);
    // Older files did not have auto-refresh settings; migration supplies those defaults.
    st::atomic_write(root/"signals.ini","rsi_oversold_enabled=1\nrsi_oversold_below=25\nrequire_all=1\n");SignalSettings settings;st::load_signal_settings(root,settings);
    check(settings.rsi_oversold_below==25&&settings.require_all&&st::read_text(root/"signals.ini")->starts_with("MOEXWATCH_SIGNALS;1;"),"legacy settings migrate with known defaults");
    const auto settings_bytes=*st::read_text(root/"signals.ini");SignalSettings settings_restart;st::load_signal_settings(root,settings_restart);check(settings_restart.rsi_oversold_below==25,"settings survive restart");
    for(const auto* bad:{"rsi_oversold_enabled=2\n","rsi_oversold_below=nan\n","require_all=1\nrequire_all=0\n","unknown=1\n","auto_refresh_minutes=1\n"}){st::atomic_write(root/"signals.ini",bad);rejects([&]{st::load_signal_settings(root,settings_restart);});check(settings_restart.rsi_oversold_below==25&&st::read_text(root/"signals.ini")==bad,"invalid settings are reported without resetting or overwriting");}
    st::atomic_write(root/"signals.ini",settings_bytes);
    Temp blocked;st::atomic_write(blocked.root/"watchlist.csv",old_watch);st::atomic_write(blocked.root/"backups","blocked");std::vector<Instrument> untouched;
    rejects([&]{st::load_watchlist(blocked.root,untouched);});check(untouched.empty()&&st::read_text(blocked.root/"watchlist.csv")==old_watch,"backup failure prevents migration and application");
    // Complete import -> journal save -> fresh load, including position levels and calculations.
    const auto identified=identify_portfolio({{"2026-07-01","SBER",PortfolioSide::Buy,10,100,1},{"2026-07-02","SBER",PortfolioSide::Sell,3,120,1}});
    const auto csv=export_portfolio_csv(identified);std::string error;
    const auto imported=prepare_portfolio_import(preview_portfolio_csv(csv,{}),{},false,error);check(imported.has_value(),"CSV import valid");
    PortfolioJournal journal; journal.transactions=*imported;journal.sequence_watermark=2;journal.levels={{"SBER","2026-07-01",90,130,identified[0].position_id}};
    st::save_portfolio_journal(root,journal);const auto loaded=st::load_portfolio_journal(root);
    check(export_portfolio_csv(loaded.transactions)==csv&&loaded.levels[0].position_id==journal.levels[0].position_id&&loaded.levels[0].stop_loss==90,"restart preserves IDs, order, levels and operations");
    const std::vector<Quote> quotes{{"SBER",110,0,"",""}};
    check(calculate_portfolio(loaded.transactions,quotes)->total_realized_profit==calculate_portfolio(journal.transactions,quotes)->total_realized_profit&&calculate_portfolio(loaded.transactions,quotes)->total_unrealized_profit==calculate_portfolio(journal.transactions,quotes)->total_unrealized_profit,"realized and unrealized results unchanged");
    check(!prepare_portfolio_import(preview_portfolio_csv(csv,loaded.transactions),loaded.transactions,true,error),"reimport after restart does not duplicate IDs");
    std::stop_source cancel;cancel.request_stop();rejects([&]{st::create_backup(root,cancel.get_token());});
    std::cout<<"storage_integration_tests: migration, corruption and import-save-restart passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
