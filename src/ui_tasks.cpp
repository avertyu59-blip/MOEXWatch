#include "ui_app.hpp"
#ifdef _WIN32
#include <type_traits>
namespace moexwatch::ui {
Application::~Application(){shutdown_tasks();}
FetchContext Application::fetch_context(std::uint64_t request) {
    const auto root=storage::data_directory();
    if(!log)log=std::make_shared<DiagnosticLog>(root/L"logs"/L"diagnostic.log");
    return {{},winhttp_get,log,request,root};
}
void Application::update_task_controls() {
    const bool busy=tasks.running(TaskKind::Market)||tasks.running(TaskKind::Dividends)||tasks.running(TaskKind::Backtest);
    if(IsWindow(shell.cancel_loading))EnableWindow(shell.cancel_loading,busy&&!shell.closing);
}
void Application::cancel_loading() {
    dividends_page.pending_refresh=false;
    for(const auto kind:{TaskKind::Market,TaskKind::Dividends,TaskKind::Backtest})tasks.cancel(kind);
    if(state.market.refreshing())SetWindowTextW(charts_page.status,L"Отменяю загрузку…");
    if(log)log->record(0,"application","cancel_requested");
    update_task_controls();
}
void Application::shutdown_tasks() {
    if(shell.closing)return;
    shell.closing=true;dividends_page.pending_refresh=false;
    mailbox->close();tasks.shutdown();
    if(IsWindow(shell.window)){KillTimer(shell.window,TIMER_TASKS);KillTimer(shell.window,TIMER_AUTO_REFRESH);}
}
void Application::drain_completions() {
    if(shell.closing)return;
    for(auto& event:mailbox->take())std::visit([&](auto& result){
        using T=std::decay_t<decltype(result)>;
        if constexpr(std::is_same_v<T,MarketRefreshResult>){
            result.cancelled=result.cancelled||tasks.stop_requested(TaskKind::Market);
            apply_market_refresh(event.request,std::move(result));
        }else if constexpr(std::is_same_v<T,DividendFetchResult>){
            result.cancelled=result.cancelled||tasks.stop_requested(TaskKind::Dividends);
            apply_dividend_response(event.request,std::move(result));
        }else if constexpr(std::is_same_v<T,BacktestResult>){
            if(result.cancelled||tasks.stop_requested(TaskKind::Backtest)){result.text="Тест отменён.";result.cancelled=true;}
            complete_backtest(reinterpret_cast<LPARAM>(new BacktestUiResult(std::move(result))));
        }else if constexpr(std::is_same_v<T,BackupResult>){
            handle_window_message(shell.window,WM_BACKUP_READY,0,reinterpret_cast<LPARAM>(new BackupResult(std::move(result))));
        }else if constexpr(std::is_same_v<T,FetchProgress>){
            if(state.market.is_current(event.request))handle_window_message(shell.window,WM_FETCH_PROGRESS,result.current,static_cast<LPARAM>(result.total));
        }else if constexpr(std::is_same_v<T,TaskFailure>){
            if(log)log->record(event.request,"worker",result.cancelled?"cancelled":"failed");
            switch(result.kind){
            case TaskKind::Market:state.market.cancel_refresh();EnableWindow(GetDlgItem(shell.window,ID_REFRESH),TRUE);SetWindowTextW(charts_page.status,L"Загрузка прервана. Предыдущие данные сохранены.");break;
            case TaskKind::Dividends:dividends_page.pending_refresh=false;EnableWindow(dividends_page.refresh,TRUE);SetWindowTextW(dividends_page.info,L"Загрузка событий прервана.");break;
            case TaskKind::Backtest:backtest_page.busy=false;EnableWindow(backtest_page.run,TRUE);SetWindowTextW(backtest_page.result,L"Тест прерван.");break;
            case TaskKind::Backup:EnableWindow(shell.backup_button,TRUE);MessageBoxW(shell.window,L"Не удалось создать резервную копию.",L"MOEXWatch",MB_ICONERROR);break;
            default:break;
            }
        }
    },event.data);
    update_task_controls();
}
void Application::open_diagnostics() {
    try{
        if(!log)(void)fetch_context(0);
        log->record(0,"application","open_log");
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(shell.window,L"open",log->path().c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
            throw std::runtime_error("Не удалось открыть диагностический журнал.");
    }catch(const std::exception& ex){MessageBoxW(shell.window,widen(ex.what()).c_str(),L"MOEXWatch",MB_ICONERROR);}
}
}
#endif
