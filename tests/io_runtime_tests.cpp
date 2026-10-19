#include "async_tasks.hpp"
#include "market_validation.hpp"
#include <iostream>
#include <fstream>
#include <future>
using namespace moexwatch;
using namespace std::chrono_literals;
namespace {
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class E,class F> void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("expected rejection");}
}
int main(){try{
    HttpRequest request{"example.invalid","/secret?account=123","test"};
    int calls=0;
    auto flaky=[&](const HttpRequest&,std::stop_token){return HttpResponse{++calls<3?503U:200U,"ok"};};
    check(get_with_retry(request,flaky,{},nullptr,7,{3,0ms}).body=="ok"&&calls==3,"transient errors recover within three attempts");
    calls=0;rejects<NetworkFailure>([&]{get_with_retry(request,[&](auto&,auto){++calls;return HttpResponse{403,"denied"};},{},nullptr,0,{3,0ms});});check(calls==1,"403 is not retried");
    calls=0;rejects<NetworkFailure>([&]{get_with_retry(request,[&](auto&,auto){++calls;return HttpResponse{302,""};},{},nullptr,0,{3,0ms});});check(calls==1,"redirect rejected without retry");
    calls=0;rejects<NetworkFailure>([&]{get_with_retry(request,[&](auto&,auto)->HttpResponse{++calls;throw NetworkFailure("timeout",true,12002);},{},nullptr,0,{3,0ms});});check(calls==3,"transport retries bounded");
    calls=0;rejects<ValidationFailure>([&]{get_with_retry(request,[&](auto&,auto)->HttpResponse{++calls;throw ValidationFailure("bad payload");},{},nullptr,0,{3,0ms});});check(calls==1,"invalid payload not retried");
    std::stop_source cancelled;cancelled.request_stop();
    rejects<Cancelled>([&]{get_with_retry(request,flaky,cancelled.get_token());});
    std::promise<void> attempted;std::atomic<int> attempts{};
    const auto start=std::chrono::steady_clock::now();
    std::jthread waiting([&](std::stop_token stop){rejects<Cancelled>([&]{get_with_retry(request,[&](auto&,auto){++attempts;attempted.set_value();return HttpResponse{429,"wait"};},stop);});});
    attempted.get_future().wait();waiting.request_stop();waiting.join();
    check(attempts==1&&std::chrono::steady_clock::now()-start<1s,"backoff immediately cancellable");
    const auto root=std::filesystem::temp_directory_path()/("moex-io-"+std::to_string(start.time_since_epoch().count()));
    std::filesystem::create_directories(root);
    DiagnosticLog log(root/"diagnostic.log",512);
    for(int i=0;i<30;++i)log.record(9,"moex","receive_ok",1,200,3);
    check(std::filesystem::exists(root/"diagnostic.log.2"),"log rotates twice");
    for(const auto& entry:std::filesystem::directory_iterator(root))check(entry.file_size()<=512,"log bounded");
    get_with_retry(request,[](auto&,auto){return HttpResponse{200,"private body 123"};},{},&log);
    std::ifstream input(root/"diagnostic.log");std::string text((std::istreambuf_iterator<char>(input)),{});
    check(text.find("secret")==text.npos&&text.find("123")==text.npos&&text.find("private")==text.npos,"log excludes endpoint, query and body");
    {
        TaskGroup tasks;auto mailbox=std::make_shared<CompletionMailbox>();std::promise<void> entered;std::atomic<bool> exited{};
        check(tasks.start(TaskKind::Market,mailbox,[&](std::stop_token stop)->BackgroundEvent{entered.set_value();cancellable_wait(stop,10s);exited=true;check_cancelled(stop);return {17,MarketRefreshResult{}};}),"worker starts");
        entered.get_future().wait();check(!tasks.start(TaskKind::Market,mailbox,[](auto){return BackgroundEvent{};}),"overlap blocked");
        tasks.cancel(TaskKind::Market);tasks.shutdown();check(exited&&!tasks.running(TaskKind::Market),"shutdown stops and joins worker");
        auto events=mailbox->take();check(events.size()==1&&std::get<TaskFailure>(events[0].data).cancelled,"cancelled completion typed");
        check(tasks.start(TaskKind::Market,mailbox,[](auto){return BackgroundEvent{18,MarketRefreshResult{}};}),"finished slot reusable");tasks.shutdown();
        check(mailbox->take().size()==1,"new completion retained");mailbox->close();check(!mailbox->publish({}),"closed mailbox drops late replies");
    }
    for(const auto kind:{TaskKind::Market,TaskKind::Dividends,TaskKind::Backtest,TaskKind::Backup}){
        TaskGroup tasks;auto box=std::make_shared<CompletionMailbox>();std::promise<void> entered;
        tasks.start(kind,box,[&](std::stop_token stop)->BackgroundEvent{entered.set_value();cancellable_wait(stop,10s);check_cancelled(stop);return {};});
        entered.get_future().wait();box->close();tasks.shutdown();check(!tasks.running(kind)&&box->take().empty(),"each module stops without publishing to closed mailbox");
        tasks.start(kind,box,[](auto)->BackgroundEvent{throw std::runtime_error("injected failure");});tasks.shutdown();check(!tasks.running(kind),"exception cannot leave module marked running");
    }
    check(validate_quote_response("SECID;LAST;LASTCHANGEPRCNT;UPDATETIME;SYSTIME\nSBER;100;1;12:00;2026-10-16 12:00\n","SBER").last==100,"valid quote accepted");
    rejects<ValidationFailure>([]{validate_quote_response("SECID;LAST\nGAZP;100\n","SBER");});
    const std::string header="begin;open;high;low;close;volume\n";
    check(validate_candle_page(header).empty(),"valid empty pagination terminator");
    check(validate_candle_page(header+"2026-10-15 00:00:00;100;110;90;105;1000\n").size()==1,"valid OHLC accepted");
    for(const auto& bad:{"2026-02-30;100;110;90;105;1000\n","2026-10-15;100;99;90;105;1000\n","2026-10-15;100;110;90;105;-1\n","2026-10-15;broken;110;90;105;1000\n"})rejects<ValidationFailure>([&]{validate_candle_page(header+bad);});
    rejects<ValidationFailure>([&]{validate_candle_page(header+"2026-10-15;100;110;90;105;1000\n2026-10-15;100;110;90;105;1000\n");});
    input.close();
#ifndef _WIN32
    const auto victim=root/"external.txt";
    {std::ofstream file(victim);file<<"unchanged";}
    for(const auto& suffix:{"", ".1", ".2"}) {
        const auto path=root/(std::string("linked.log")+suffix);
        std::filesystem::create_symlink(victim,path);
        DiagnosticLog linked(root/"linked.log",512);
        linked.record(1,"moex","receive_ok");
        std::ifstream file(victim);std::string bytes((std::istreambuf_iterator<char>(file)),{});
        check(bytes=="unchanged","diagnostics never write through a file symlink");
        check(!std::filesystem::exists(root/"linked.log")||std::filesystem::is_symlink(root/"linked.log"),"unsafe rotation path blocks logging");
        std::filesystem::remove(path);
    }
#endif
    std::filesystem::remove_all(root);
    std::cout<<"io_runtime_tests: retries, cancellation, lifecycle, diagnostics and validation passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
