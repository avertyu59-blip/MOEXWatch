#include "ui_app.hpp"
#include <iostream>
#include <stdexcept>
using namespace moexwatch;
using namespace moexwatch::ui;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
struct Fixture {
    Application app;
    Fixture(){
        app.app_window=CreateWindowExW(0,L"STATIC",L"MOEXWatch page tests",WS_OVERLAPPEDWINDOW,0,0,1340,870,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        check(app.app_window!=nullptr,"hidden owner created");
        app.app_storage_blocked=true; // No real account files, network, or notifications in this fixture.
        app.create_controls(app.app_window);
    }
    ~Fixture(){KillTimer(app.app_window,TIMER_AUTO_REFRESH);DestroyWindow(app.app_window);if(app.app_font)DeleteObject(app.app_font);}
};
RECT relative(HWND control,HWND parent){RECT r{};GetWindowRect(control,&r);MapWindowPoints(HWND_DESKTOP,parent,reinterpret_cast<POINT*>(&r),2);return r;}
}
int main(){try{
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES};
    check(InitCommonControlsEx(&common)!=FALSE,"common controls initialized");
    Fixture fixture;auto& app=fixture.app;
    check(TabCtrl_GetItemCount(app.app_tabs)==6,"all six pages created");
    app.activate_page(4);app.resize_backtest(1164,720);
    check(app.narrow(app.read_edit(app.backtest_page.capital))=="100000","default capital preserved");
    check(app.narrow(app.read_edit(app.backtest_page.fee))=="0.05","default commission preserved");
    check(IsWindowEnabled(app.backtest_page.run)!=FALSE,"run enabled initially");
    const auto run=relative(app.backtest_page.run,app.app_window);
    const auto refresh=relative(GetDlgItem(app.app_window,ID_REFRESH),app.app_window);
    RECT overlap{};check(!IntersectRect(&overlap,&run,&refresh),"run and refresh buttons do not overlap");
    check(run.left>=0&&run.right<=1164,"run button fits minimum client width");
    auto* result=new BacktestUiResult{"Контрольный результат",false};
    app.backtest_page.busy=true;EnableWindow(app.backtest_page.run,FALSE);
    app.complete_backtest(reinterpret_cast<LPARAM>(result));
    check(!app.backtest_page.busy&&IsWindowEnabled(app.backtest_page.run),"completion restores run state");
    check(app.narrow(app.read_edit(app.backtest_page.result))=="Контрольный результат","completion displays Unicode result");
    app.activate_page(0);app.activate_page(4);
    check(app.narrow(app.read_edit(app.backtest_page.result))=="Контрольный результат","switching tabs preserves test result");
    check(!app.command_backtest(MAKEWPARAM(ID_PORT_ADD,0)),"backtest leaves unrelated commands to coordinator");
    app.signals_page.settings.rsi_oversold_enabled=false;
    app.signals_page.settings.rsi_overbought_enabled=false;
    app.signals_page.settings.above_sma200_enabled=false;
    app.signals_page.settings.near_target_enabled=true;
    app.signals_page.settings.notifications_enabled=false;
    app.synchronize_signal_controls();
    app.signals_page.baseline_ready=true;
    CheckDlgButton(app.app_window,ID_SIG_NOTIFY,BST_CHECKED);
    check(app.read_signal_settings_from_controls(),"valid signal settings read from real controls");
    check(app.signals_page.baseline_ready,"popup-only edit preserves technical baseline");
    CheckDlgButton(app.app_window,ID_SIG_NOTIFY,BST_UNCHECKED);
    check(app.read_signal_settings_from_controls(),"popups can be disabled without discarding conditions");
    app.activate_page(3);app.resize_signals(1164,720);
    check(relative(GetDlgItem(app.app_window,ID_SIG_SAVE),app.app_window).right<=1164,"save conditions fits minimum client width");
    Instrument instrument;instrument.ticker="SBER";instrument.target=200;
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto stamp=observation_time(now);
    instrument.quote={"SBER",200,0,stamp.substr(11),stamp};instrument.quote_stale=false;instrument.candles_stale=false;
    instrument.candles={{"2026-10-15",190,205,185,200,1000}};
    app.app_items={instrument};app.fill_signals();
    check(ListView_GetItemCount(app.signals_page.list)==1,"technical match still displayed after extraction");
    app.signals_page.observation_writable=false;
    app.signals_page.observer.configure({{1,"SBER",true,190,true}});
    app.notify_new_signals();check(app.signals_page.history.empty(),"first observation establishes baseline");
    app.app_items[0].quote.last=180;app.notify_new_signals();
    check(app.signals_page.history.size()==1&&app.signals_page.history[0].kind=="Цена","price crossing recorded with popups disabled");
    app.notify_new_signals();check(app.signals_page.history.size()==1,"repeat refresh does not duplicate event");
    app.activate_page(5);app.activate_page(3);
    check(app.signals_page.history.size()==1,"history survives tab switching");
    check(!app.command_signals(MAKEWPARAM(ID_PORT_ADD,0)),"signal page leaves unrelated commands alone");
    SendMessageW(app.portfolio_page.ticker,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"SBER"));
    SendMessageW(app.portfolio_page.ticker,CB_SETCURSEL,0,0);
    const auto enter=[&](const wchar_t* quantity,const wchar_t* price,int side){
        SetWindowTextW(app.portfolio_page.date,L"2026-10-15");
        SetWindowTextW(app.portfolio_page.quantity,quantity);
        SetWindowTextW(app.portfolio_page.price,price);
        SetWindowTextW(app.portfolio_page.commission,L"0");
        SendMessageW(app.portfolio_page.side,CB_SETCURSEL,static_cast<WPARAM>(side),0);
        check(app.command_portfolio(MAKEWPARAM(ID_PORT_ADD,0)),"portfolio handles add operation command");
    };
    enter(L"10",L"100",0);enter(L"10",L"200",0);enter(L"5",L"180",1);
    const auto position=app.open_position_for_ticker("SBER");
    check(position&&position->quantity==15&&position->average_entry_price==150&&position->realized_profit==150,"UI operations preserve averaging and partial sale calculations");
    check(ListView_GetItemCount(app.portfolio_page.positions)==1&&ListView_GetItemCount(app.portfolio_page.transactions_list)==3,"positions and operation rows refresh together");
    ListView_SetItemState(app.portfolio_page.positions,0,LVIS_SELECTED,LVIS_SELECTED);
    SetWindowTextW(app.portfolio_page.position_stop,L"110");SetWindowTextW(app.portfolio_page.position_take,L"210");
    check(app.command_portfolio(MAKEWPARAM(ID_POSITION_LEVELS_SAVE,0)),"portfolio handles save levels command");
    const auto* levels=app.levels_for_position("SBER",position->opened_on);
    check(levels&&levels->stop_loss==110&&levels->take_profit==210,"levels remain tied to selected open position");
    app.app_table_sorts[ID_PORT_TRANSACTIONS]={4,false};app.apply_table_sort(app.portfolio_page.transactions_list,ID_PORT_TRANSACTIONS);
    check(row_data(app.portfolio_page.transactions_list,0)==1,"highest-priced purchase moves to first displayed row");
    ListView_SetItemState(app.portfolio_page.transactions_list,0,LVIS_SELECTED,LVIS_SELECTED);
    check(app.command_portfolio(MAKEWPARAM(ID_PORT_DELETE,0)),"portfolio handles deletion command");
    check(app.portfolio_page.transactions.size()==2&&app.portfolio_page.transactions[0].price==100&&app.portfolio_page.transactions[1].side==PortfolioSide::Sell,"delete after sorting removes selected model operation");
    app.resize_portfolio(1164,720);app.activate_page(2);app.activate_page(0);app.activate_page(2);
    check(app.portfolio_page.transactions.size()==2&&ListView_GetItemCount(app.portfolio_page.transactions_list)==2,"portfolio survives resizing and tab switching");
    const auto journal=relative(app.portfolio_page.transactions_list,app.app_window);
    check(journal.right<=1164&&journal.bottom<=720,"operation table fits minimum client size");
    check(!app.command_portfolio(MAKEWPARAM(ID_BACKTEST_RUN,0)),"portfolio leaves backtest commands alone");
    std::cout<<"ui_pages_tests: backtest, signals and portfolio checks passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
