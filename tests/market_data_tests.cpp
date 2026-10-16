#include "market_data.hpp"
#include "storage.hpp"
#include <fstream>
#include <iostream>
using namespace moexwatch;
namespace st=moexwatch::storage;
namespace {
void check(bool b,const char* reason){if(!b)throw std::runtime_error(reason);}
template<class E,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("expected typed failure");}
std::string fixture(const char* filename){std::ifstream in(std::filesystem::path(FIXTURE_DIR)/filename,std::ios::binary);if(!in)throw std::runtime_error("fixture unavailable");return {(std::istreambuf_iterator<char>(in)),{}};}
struct Temp {std::filesystem::path root=std::filesystem::temp_directory_path()/("moex-fetch-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));Temp(){std::filesystem::create_directories(root);}~Temp(){std::error_code e;std::filesystem::remove_all(root,e);}};
const std::string header="begin;open;high;low;close;volume\n";
const std::string bars=header+"2026-10-14 00:00:00;100;110;90;105;1000\n2026-10-15 00:00:00;105;115;100;110;1200\n";
const std::string quote="SECID;LAST;LASTCHANGEPRCNT;UPDATETIME;SYSTIME\nSBER;110;1;12:00:00;2026-10-16 12:00:00\n";
}
int main(){try{
    Temp temp;int candle_requests=0,quote_requests=0;
    FetchContext context{{},[&](const HttpRequest& req,std::stop_token){if(req.path.find("candles.csv")!=std::string::npos){++candle_requests;return HttpResponse{200,req.path.ends_with("start=0")?bars:header};}++quote_requests;return HttpResponse{200,quote};},std::make_shared<DiagnosticLog>(temp.root/"logs"/"test.log"),41,temp.root};
    Instrument old;old.ticker="SBER";old.quote.ticker="SBER";old.quote.last=90;
    const auto loaded=fetch_instrument(old,context);
    check(loaded.quote.last==110&&!loaded.quote_stale&&!loaded.candles_stale&&loaded.candles.size()==2,"fetch validates and returns independent fresh model");
    check(old.quote.last==90&&old.candles.empty()&&quote_requests==1&&candle_requests==2,"fetch does not mutate UI source and terminates pagination");
    check(st::load_candles(temp.root,"SBER")->candles.back().close==110,"only complete validated history committed");
    const auto cachefile=temp.root/"cache"/"SBER-TQBR-D1.csv";const auto saved=*st::read_text(cachefile);
    int bad_requests=0;
    context.transport=[&](const HttpRequest& req,std::stop_token){if(req.path.find("candles.csv")!=std::string::npos){++bad_requests;return HttpResponse{200,header+"2026-10-16;100;50;90;105;1000\n"};}return HttpResponse{200,quote};};
    const auto invalid=fetch_instrument(loaded,context);
    check(invalid.candles_stale&&invalid.candles.back().close==110&&bad_requests==1&&st::read_text(cachefile)==saved,"invalid history is not retried, applied or cached");
    context.transport=[&](const HttpRequest& req,std::stop_token){if(req.path.find("candles.csv")!=std::string::npos)return HttpResponse{200,req.path.ends_with("start=0")?bars:header};return HttpResponse{200,"SECID;LAST;LASTCHANGEPRCNT;UPDATETIME;SYSTIME\nGAZP;1;0;;\n"};};
    const auto wrong=fetch_instrument(loaded,context);check(wrong.quote_stale&&wrong.quote.last==110&&!wrong.error.empty(),"wrong ticker cannot overwrite quote");
    const auto before_cancel=*st::read_text(cachefile);std::stop_source cancel;
    context.stop=cancel.get_token();context.transport=[&](const HttpRequest&,std::stop_token){cancel.request_stop();return HttpResponse{200,quote};};
    rejects<Cancelled>([&]{fetch_instrument(loaded,context);});check(st::read_text(cachefile)==before_cancel,"cancelled instrument fetch preserves previous cache");
    context.stop={};context.transport=[&](const HttpRequest& req,std::stop_token){return HttpResponse{200,req.path.ends_with("start=0")?bars:header};};
    check(fetch_index_candles(context).size()==2&&st::load_candles(temp.root,"IMOEX").has_value(),"index uses independent validated cache");
    context.transport=[&](const HttpRequest& req,std::stop_token){return HttpResponse{200,fixture(req.path=="/dividends/"?"smartlab_calendar.html":"smartlab_sber.html")};};
    DividendFetchResult dividends;dividends.ticker="SBER";dividends.checked_at="2026-10-16 12:00";
    fetch_dividend_events(dividends,context);check(!dividends.events.empty(),"dividend provider receives and validates both pages");
    int dividend_calls=0;std::stop_source dividend_cancel;context.stop=dividend_cancel.get_token();
    context.transport=[&](const HttpRequest&,std::stop_token){if(++dividend_calls==2)dividend_cancel.request_stop();return HttpResponse{200,fixture("smartlab_sber.html")};};
    DividendFetchResult partial;partial.ticker="SBER";
    rejects<Cancelled>([&]{fetch_dividend_events(partial,context);});check(partial.events.empty(),"partial dividend refresh never becomes a complete snapshot");
    std::cout<<"market_data_tests: injected quote, index, candle and dividend pipelines passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
