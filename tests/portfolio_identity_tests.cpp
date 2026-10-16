#include "portfolio_identity.hpp"
#include "portfolio_csv.hpp"
#include "storage.hpp"
#include "dividend_calculations.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace moexwatch;
namespace st=moexwatch::storage;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class F>bool fails(F f){try{f();return false;}catch(const std::exception&){return true;}}
struct Temp{std::filesystem::path root=std::filesystem::temp_directory_path()/("moexwatch-identity-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));Temp(){std::filesystem::create_directories(root);}~Temp(){std::error_code ignored;std::filesystem::remove_all(root,ignored);}};
}
int main(){try{
    const std::vector<PortfolioTransaction> legacy{{"2026-07-01","SBER",PortfolioSide::Buy,10,100,1},
        {"2026-07-01","SBER",PortfolioSide::Sell,10,110,1},
        {"2026-07-01","SBER",PortfolioSide::Buy,5,120,1},
        {"2026-07-02","SBER",PortfolioSide::Buy,5,140,1}};
    const auto original=calculate_portfolio(legacy,{});check(original.has_value(),"legacy profit fixture");
    auto identified=identify_portfolio(legacy);
    for(const auto& t:identified)check(valid_uuid(t.id)&&valid_uuid(t.position_id)&&t.sequence>0,"every operation has persistent identity");
    check(identified[0].id!=identified[1].id&&identified[0].position_id==identified[1].position_id&&identified[1].position_id!=identified[2].position_id,"same-day close and reopen creates independent cycle");
    check(identified[2].position_id==identified[3].position_id,"averaging preserves cycle");
    const auto current=calculate_portfolio(identified,{});check(current.has_value(),"identified portfolio is calculable");check(current->total_realized_profit==original->total_realized_profit&&current->positions[0].average_entry_price==original->positions[0].average_entry_price,"migration preserves profit and cost basis");
    const auto again=identify_portfolio(identified);check(export_portfolio_csv(again)==export_portfolio_csv(identified),"normalization never changes existing identity");
    auto reordered=identified;std::reverse(reordered.begin(),reordered.end());
    const auto reordered_result=calculate_portfolio(reordered,{});check(reordered_result.has_value(),"reordered persistent history is calculable");
    check(reordered_result->positions[0].average_entry_price==current->positions[0].average_entry_price,"stored sequence survives arbitrary vector order");
    const auto csv=export_portfolio_csv(reordered);std::string error;
    auto restored=prepare_portfolio_import(preview_portfolio_csv(csv,{}),{},false,error);
    check(restored&&export_portfolio_csv(*restored)==export_portfolio_csv(identified),"export/import preserves IDs, order and values");
    check(!prepare_portfolio_import(preview_portfolio_csv(csv,identified),identified,true,error),"same ID always skipped, even with include duplicates");
    auto conflict=identified;conflict[0].price=101;
    const auto bad_preview=preview_portfolio_csv(export_portfolio_csv(conflict),identified);
    check(bad_preview.rows[0].status==ImportStatus::Error&&!prepare_portfolio_import(bad_preview,identified,true,error),"same ID with different contents blocks import");
    auto partial=identified;partial.pop_back();
    partial.push_back({"2026-07-02","GAZP",PortfolioSide::Buy,1,100,0});partial=identify_portfolio(partial,4);
    auto recovered=prepare_portfolio_import(preview_portfolio_csv(export_portfolio_csv(identified),partial),partial,false,error,5);
    check(recovered&&recovered->size()==5&&std::any_of(recovered->begin(),recovered->end(),[&](const auto& t){return t.id==identified[3].id&&t.position_id==identified[3].position_id;}),"deleted operation recovers its original identity after newer unrelated operations");
    const auto one=identify_portfolio({legacy.front()});auto distinct=one;
    distinct[0].id=new_portfolio_id();distinct[0].sequence=2;
    const auto distinct_preview=preview_portfolio_csv(export_portfolio_csv(distinct),one);
    check(distinct_preview.rows[0].status==ImportStatus::New,"different ID with identical prices is a distinct operation");
    const auto distinct_result=prepare_portfolio_import(distinct_preview,one,false,error);check(distinct_result.has_value(),"distinct identified import is valid");
    check(distinct_result->size()==2,"distinct identical operation is not suppressed by old heuristic");
    auto collision=identified;collision[1].id=collision[0].id;check(!position_cycles(collision),"duplicate operation ID rejected");
    collision=identified;collision[1].sequence=collision[0].sequence;check(!position_cycles(collision),"ambiguous order rejected");
    collision=identified;collision[2].position_id=collision[0].position_id;check(!position_cycles(collision),"closed position ID cannot be recycled");
    auto removed=identified;removed.erase(removed.begin()+2);removed=identify_portfolio(removed);
    check(removed.back().position_id==identified[2].position_id,"removing original opening buy preserves ID through surviving buy");
    removed=identified;removed.erase(removed.begin()+1);
    check(fails([&]{identify_portfolio(removed);}),"removing closing sale cannot silently merge position IDs");
    removed=identified;removed.erase(removed.begin());
    check(fails([&]{identify_portfolio(removed);}),"removing buy cannot create oversell");
    const std::vector<PositionLevels> old_levels{{"SBER","2026-07-01",90,150}};
    auto migration=migrate_portfolio_identity(legacy,old_levels);
    check(migration.levels.empty()&&migration.unresolved_levels.size()==1,"ambiguous old date is preserved unresolved");
    const auto single=migrate_portfolio_identity({legacy.front()},old_levels);
    check(single.levels.size()==1&&single.levels[0].position_id==single.transactions[0].position_id,"unambiguous old level gets correct position ID");
    Temp tmp;st::save_portfolio(tmp.root,legacy);st::save_position_levels(tmp.root,old_levels);
    const auto old_bytes=st::read_text(tmp.root/"portfolio_transactions.csv");
    check(fails([&]{st::load_portfolio_journal(tmp.root,false);})&&!std::filesystem::exists(tmp.root/"portfolio_journal_v2.csv"),"blocked storage prevents migration writes");
    auto loaded=st::load_portfolio_journal(tmp.root);
    check(loaded.transactions.size()==4&&loaded.unresolved_levels.size()==1,"legacy files migrate together");
    check(st::read_text(tmp.root/"portfolio_transactions.csv")==old_bytes,"legacy journal remains untouched");
    bool backup=false;for(const auto& entry:std::filesystem::recursive_directory_iterator(tmp.root/"backups"))if(entry.path().filename()=="portfolio_transactions.csv")backup=true;
    check(backup,"migration creates backup before writing canonical journal");
    auto loaded_again=st::load_portfolio_journal(tmp.root);
    check(export_portfolio_csv(loaded.transactions)==export_portfolio_csv(loaded_again.transactions),"restart preserves every ID");
    loaded.levels={{"SBER","2026-07-01",115,160,loaded.transactions[2].position_id}};
    st::save_portfolio_journal(tmp.root,loaded);loaded_again=st::load_portfolio_journal(tmp.root);
    check(loaded_again.levels[0].position_id==loaded.transactions[2].position_id,"levels round trip by cycle ID");
    const auto valid_bytes=*st::read_text(tmp.root/"portfolio_journal_v2.csv");
    st::atomic_write(tmp.root/"portfolio_journal_v2.csv",valid_bytes.substr(0,valid_bytes.find("END;")));
    check(fails([&]{st::load_portfolio_journal(tmp.root);}),"truncated new journal cannot fall back to old files");
    check(st::read_text(tmp.root/"portfolio_transactions.csv")==old_bytes,"corrupt canonical file does not overwrite legacy recovery source");
    st::atomic_write(tmp.root/"portfolio_journal_v2.csv",valid_bytes);
    auto emptied=loaded_again;emptied.transactions.clear();st::save_portfolio_journal(tmp.root,emptied);
    const auto empty_restart=st::load_portfolio_journal(tmp.root);
    check(empty_restart.sequence_watermark>=4,"deleting operations preserves sequence watermark across restart");
    const auto later=identify_portfolio({legacy[0]},empty_restart.sequence_watermark);
    check(later[0].sequence>empty_restart.sequence_watermark,"new operation cannot recycle deleted order");
    st::atomic_write(tmp.root/"portfolio_journal_v2.csv",valid_bytes);
    const auto backup_path=st::create_backup(tmp.root);check(std::filesystem::exists(backup_path/"portfolio_journal_v2.csv"),"regular backup contains canonical journal");
    Temp blocked;st::save_portfolio(blocked.root,legacy);
    st::atomic_write(blocked.root/"backups","blocks backup directory");
    check(fails([&]{st::load_portfolio_journal(blocked.root);})&&!std::filesystem::exists(blocked.root/"portfolio_journal_v2.csv"),"failed backup prevents migration write");
    DividendEvent event;event.record_date="2026-07-02";event.amount_per_share=10;event.currency="RUB";event.status=DividendStatus::Approved;
    const std::vector<TradingDay> calendar{{"2026-07-01",true,""},{"2026-07-02",true,""}};
    auto entitlement=calculate_dividend_entitlement("SBER",event,identified,calendar,"2026-07-03");
    check(entitlement.positions.size()==1&&entitlement.positions[0].position_id==identified[2].position_id&&entitlement.positions[0].quantity==5,"dividend cutoff identifies correct same-day reopened cycle");
    auto closed_later=identified;closed_later.push_back({"2026-07-03","SBER",PortfolioSide::Sell,10,150,0});closed_later=identify_portfolio(closed_later);
    entitlement=calculate_dividend_entitlement("SBER",event,closed_later,calendar,"2026-07-04");
    check(entitlement.positions.size()==1&&entitlement.positions[0].position_id==identified[2].position_id&&entitlement.positions[0].gross_amount==50,"closed-after-cutoff cycle retains dividend entitlement identity");
    std::cout<<"portfolio_identity_tests: all checks passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
