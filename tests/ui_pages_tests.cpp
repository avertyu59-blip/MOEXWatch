#include "ui_app.hpp"
#include <iostream>
#include <stdexcept>
#include <set>
#include <cstdint>
using namespace moexwatch;
using namespace moexwatch::ui;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
struct Fixture {
    Application app;
    Fixture(){
        app.shell.window=CreateWindowExW(0,L"STATIC",L"MOEXWatch page tests",WS_OVERLAPPEDWINDOW,0,0,1340,870,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        check(app.shell.window!=nullptr,"hidden owner created");
        app.state.persistence.writes_blocked=true; // No real account files, network, or notifications in this fixture.
        app.create_controls(app.shell.window);
    }
    ~Fixture(){KillTimer(app.shell.window,TIMER_AUTO_REFRESH);DestroyWindow(app.shell.window);if(app.shell.font)DeleteObject(app.shell.font);}
};
RECT relative(HWND control,HWND parent){RECT r{};GetWindowRect(control,&r);MapWindowPoints(HWND_DESKTOP,parent,reinterpret_cast<POINT*>(&r),2);return r;}
// Exercise the production instance routers without starting real I/O at WM_CREATE.
LRESULT CALLBACK router_host(HWND w,UINT m,WPARAM p,LPARAM l){
    if(m==WM_CREATE||m==WM_DESTROY)return 0;
    return Application::window_proc(w,m,p,l);
}
}
int main(){try{
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES};
    check(InitCommonControlsEx(&common)!=FALSE,"common controls initialized");
    Fixture fixture;auto& app=fixture.app;
    check(!app.charts_page.updating&&!app.portfolio_page.updating,"views start outside update scope");
    try {
        ScopedViewUpdate outer(app.charts_page.updating);
        { ScopedViewUpdate nested(app.charts_page.updating);check(app.charts_page.updating,"nested update is suppressed"); }
        check(app.charts_page.updating&&!app.portfolio_page.updating,"suppression belongs only to the current page");
        throw std::runtime_error("scope test");
    } catch(const std::runtime_error&) {}
    check(!app.charts_page.updating,"exception restores page notification state");
    check(TabCtrl_GetItemCount(app.shell.tabs)==6,"all six pages created");
    app.activate_page(4);app.resize_backtest(1164,720);
    check(app.narrow(app.read_edit(app.backtest_page.capital))=="100000","default capital preserved");
    check(app.narrow(app.read_edit(app.backtest_page.fee))=="0.05","default commission preserved");
    check(IsWindowEnabled(app.backtest_page.run)!=FALSE,"run enabled initially");
    const auto run=relative(app.backtest_page.run,app.shell.window);
    const auto refresh=relative(GetDlgItem(app.shell.window,ID_REFRESH),app.shell.window);
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
    CheckDlgButton(app.shell.window,ID_SIG_NOTIFY,BST_CHECKED);
    check(app.read_signal_settings_from_controls(),"valid signal settings read from real controls");
    check(app.signals_page.baseline_ready,"popup-only edit preserves technical baseline");
    CheckDlgButton(app.shell.window,ID_SIG_NOTIFY,BST_UNCHECKED);
    check(app.read_signal_settings_from_controls(),"popups can be disabled without discarding conditions");
    app.activate_page(3);app.resize_signals(1164,720);
    check(relative(GetDlgItem(app.shell.window,ID_SIG_SAVE),app.shell.window).right<=1164,"save conditions fits minimum client width");
    Instrument instrument;instrument.ticker="SBER";instrument.target=200;
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto stamp=observation_time(now);
    instrument.quote={"SBER",200,0,stamp.substr(11),stamp};instrument.quote_stale=false;instrument.candles_stale=false;
    instrument.candles={{"2026-10-15",190,205,185,200,1000}};
    app.state.market.instruments={instrument};app.fill_signals();
    check(ListView_GetItemCount(app.signals_page.list)==1,"technical match still displayed after extraction");
    app.signals_page.observation_writable=false;
    app.signals_page.observer.configure({{1,"SBER",true,190,true}});
    app.notify_new_signals();check(app.signals_page.history.empty(),"first observation establishes baseline");
    app.state.market.instruments[0].quote.last=180;app.notify_new_signals();
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
    app.portfolio_page.transactions_sort={4,false};app.apply_table_sort(app.portfolio_page.transactions_list,ID_PORT_TRANSACTIONS);
    check(row_data(app.portfolio_page.transactions_list,0)==1,"highest-priced purchase moves to first displayed row");
    ListView_SetItemState(app.portfolio_page.transactions_list,0,LVIS_SELECTED,LVIS_SELECTED);
    check(app.command_portfolio(MAKEWPARAM(ID_PORT_DELETE,0)),"portfolio handles deletion command");
    check(app.portfolio_page.transactions.size()==2&&app.portfolio_page.transactions[0].price==100&&app.portfolio_page.transactions[1].side==PortfolioSide::Sell,"delete after sorting removes selected model operation");
    app.resize_portfolio(1164,720);app.activate_page(2);app.activate_page(0);app.activate_page(2);
    check(app.portfolio_page.transactions.size()==2&&ListView_GetItemCount(app.portfolio_page.transactions_list)==2,"portfolio survives resizing and tab switching");
    const auto journal=relative(app.portfolio_page.transactions_list,app.shell.window);
    check(journal.right<=1164&&journal.bottom<=720,"operation table fits minimum client size");
    check(!app.command_portfolio(MAKEWPARAM(ID_BACKTEST_RUN,0)),"portfolio leaves backtest commands alone");
    // Real chart painting into a DIB exercises every GDI path without network data.
    app.state.market.instruments[0].candles.clear();app.state.market.imoex_candles.clear();
    using namespace std::chrono;
    const auto last_session=sys_days{year{2026}/10/15};
    for(int i=0;i<270;++i){
        const auto date=observation_time(duration_cast<seconds>((last_session-days{269-i}).time_since_epoch()).count()).substr(0,10);
        const double price=100.0+i*0.3+std::sin(i*0.3)*3;
        app.state.market.instruments[0].candles.push_back({date,price-1,price+2,price-2,price,i==269?5000.0:1000.0});
        const double index=3000.0+i*2;
        app.state.market.imoex_candles.push_back({date,index-1,index+2,index-2,index,10000});
    }
    app.state.market.imoex_stale=false;
    Instrument second=app.state.market.instruments[0];second.ticker="GAZP";second.quote.ticker="GAZP";second.quote.last=90;second.target=95;
    app.state.market.instruments.push_back(second);
    app.charts_page.sort={1,true};app.fill_list();
    check(row_data(app.charts_page.list,0)==1,"watch table sorts without changing instrument identity");
    ListView_SetItemState(app.charts_page.list,0,LVIS_SELECTED,LVIS_SELECTED);
    NMLISTVIEW selected{};selected.hdr.hwndFrom=app.charts_page.list;selected.hdr.idFrom=ID_LIST;selected.hdr.code=LVN_ITEMCHANGED;
    app.handle_window_message(app.shell.window,WM_NOTIFY,ID_LIST,reinterpret_cast<LPARAM>(&selected));
    check(app.charts_page.selection.ticker=="GAZP","main coordinator forwards selected stock to analytics");
    SetWindowTextW(app.charts_page.target_edit,L"150");
    app.handle_window_message(app.shell.window,WM_COMMAND,MAKEWPARAM(ID_TARGET_SET,0),0);
    check(app.state.market.instruments[1].target==150&&app.state.market.instruments[0].target==200,"target edit applies to selected stock after sorting");
    for(int range=0;range<4;++range){
        const int sessions[]={22,66,252,0};
        SendMessageW(GetDlgItem(app.shell.window,ID_RANGE),CB_SETCURSEL,static_cast<WPARAM>(range),0);
        app.handle_window_message(app.shell.window,WM_COMMAND,MAKEWPARAM(ID_RANGE,CBN_SELCHANGE),0);
        check(app.charts_page.chart_sessions==sessions[range],"range command reaches chart module");
    }
    SendMessageW(GetDlgItem(app.shell.window,ID_FILTER),CB_SETCURSEL,4,0);
    app.handle_window_message(app.shell.window,WM_COMMAND,MAKEWPARAM(ID_FILTER,CBN_SELCHANGE),0);
    check(ListView_GetItemCount(app.charts_page.list)==0&&app.state.market.instruments.size()==2,"empty filter does not remove watched stocks");
    SendMessageW(GetDlgItem(app.shell.window,ID_FILTER),CB_SETCURSEL,0,0);
    app.handle_window_message(app.shell.window,WM_COMMAND,MAKEWPARAM(ID_FILTER,CBN_SELCHANGE),0);
    check(ListView_GetItemCount(app.charts_page.list)==2,"all-stocks filter restores both rows");
    HDC dc=CreateCompatibleDC(nullptr);check(dc!=nullptr,"chart memory DC created");
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=800;bitmap.bmiHeader.biHeight=-420;bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;bitmap.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP surface=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&pixels,nullptr,0);
    check(surface!=nullptr&&pixels!=nullptr,"chart DIB created");
    const auto old_bitmap=SelectObject(dc,surface);const auto old_font=SelectObject(dc,app.shell.font);
    const auto operations_before=export_portfolio_csv(app.portfolio_page.transactions);
    const auto paint=[&](int panel,int sessions){
        TabCtrl_SetCurSel(app.charts_page.chart_tabs,panel);
        NMHDR changed{};changed.hwndFrom=app.charts_page.chart_tabs;changed.idFrom=ID_CHART_TABS;changed.code=TCN_SELCHANGE;
        app.handle_window_message(app.shell.window,WM_NOTIFY,ID_CHART_TABS,reinterpret_cast<LPARAM>(&changed));
        check(static_cast<int>(app.charts_page.chart_panel)==panel,"chart tab change is dispatched");
        app.draw_chart(dc,{0,0,800,420},0,app.charts_page.chart_panel,sessions);GdiFlush();
        std::size_t colored=0;const auto* data=static_cast<const std::uint32_t*>(pixels);
        for(int y=0;y<420;y+=4)for(int x=0;x<800;x+=4)if((data[y*800+x]&0xffffffU)!=0xffffffU)++colored;
        check(colored>50,"chart paints content instead of an empty surface");
    };
    for(int panel=0;panel<4;++panel)paint(panel,66); // warm up fonts/GDI caches
    const auto objects_before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int panel=0;panel<4;++panel)for(int sessions:{22,66,252,0})paint(panel,sessions);
    check(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=objects_before+2,"repeated chart painting does not accumulate GDI handles");
    check(export_portfolio_csv(app.portfolio_page.transactions)==operations_before,"painting does not mutate portfolio operations");
    SelectObject(dc,old_font);SelectObject(dc,old_bitmap);DeleteObject(surface);DeleteDC(dc);
    const std::vector<std::vector<HWND>*> pages={&app.charts_page.market_controls,&app.trades_page.controls,&app.portfolio_page.controls,&app.signals_page.controls,&app.backtest_page.controls};
    std::set<HWND> tracked{app.shell.tabs,app.shell.backup_button,app.signals_page.observation_page};
    for(const auto* page:pages)tracked.insert(page->begin(),page->end());
    tracked.insert(app.shell.global_controls.begin(),app.shell.global_controls.end());
    for(HWND child=GetWindow(app.shell.window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))check(tracked.contains(child),"every root control has an explicit owner");
    for(int page=0;page<6;++page){
        app.activate_page(page);
        for(std::size_t group=0;group<pages.size();++group)for(HWND control:*pages[group])check(((GetWindowLongPtrW(control,GWL_STYLE)&WS_VISIBLE)!=0)==(static_cast<int>(group)==page),"switching page shows only its controls");
        check(((GetWindowLongPtrW(app.signals_page.observation_page,GWL_STYLE)&WS_VISIBLE)!=0)==(page==5),"history visibility is coordinated");
        check((GetWindowLongPtrW(GetDlgItem(app.shell.window,ID_REFRESH),GWL_STYLE)&WS_VISIBLE)!=0,"refresh stays visible on every page");
        app.handle_window_message(app.shell.window,WM_SIZE,0,MAKELPARAM(1164,720));
    }
    check(!app.command_charts(MAKEWPARAM(ID_BACKTEST_RUN,0)),"charts do not consume backtest command");
    // A refresh is applied by ticker while another page is active; the user target wins.
    const auto selected_before_refresh=app.charts_page.selection.ticker;
    const auto period_before_refresh=app.charts_page.chart_sessions;
    app.dividends_page.ticker="SBER";
    app.activate_page(2);const auto refresh_request=app.state.market.begin_refresh();check(refresh_request.has_value(),"refresh request begins");EnableWindow(GetDlgItem(app.shell.window,ID_REFRESH),FALSE);
    auto incoming=std::make_unique<MarketRefreshResult>();
    auto refreshed=app.state.market.instruments[0];refreshed.quote.last=190;refreshed.target=999;
    incoming->instruments.push_back(refreshed);incoming->imoex_candles=app.state.market.imoex_candles;
    app.handle_window_message(app.shell.window,WM_DATA_READY,static_cast<WPARAM>(*refresh_request),reinterpret_cast<LPARAM>(incoming.release()));
    check(!app.state.market.refreshing()&&IsWindowEnabled(GetDlgItem(app.shell.window,ID_REFRESH)),"refresh completion restores global button state");
    check(app.state.market.instruments[0].quote.last==190&&app.state.market.instruments[0].target==200,"refresh updates quote but preserves user target");
    check(app.shell.active_page==2&&ListView_GetItemCount(app.portfolio_page.transactions_list)==2,"refresh updates views without changing active page or operations");
    check(app.charts_page.selection.ticker==selected_before_refresh&&app.charts_page.chart_sessions==period_before_refresh,"refresh preserves selected ticker and chart period");
    check(app.dividends_page.ticker=="SBER","analytics refresh cannot change dividend selection");
    const auto newer_request=app.state.market.begin_refresh();check(newer_request.has_value(),"newer UI request begins");
    EnableWindow(GetDlgItem(app.shell.window,ID_REFRESH),FALSE);
    auto late=std::make_unique<MarketRefreshResult>();auto late_item=app.state.market.instruments[0];late_item.quote.last=1;late->instruments.push_back(late_item);
    app.handle_window_message(app.shell.window,WM_DATA_READY,static_cast<WPARAM>(*refresh_request),reinterpret_cast<LPARAM>(late.release()));
    check(app.state.market.refreshing()&&!IsWindowEnabled(GetDlgItem(app.shell.window,ID_REFRESH))&&app.state.market.instruments[0].quote.last==190,"late response cannot mutate model or unlock newer request");
    auto valid=std::make_unique<MarketRefreshResult>();auto valid_item=app.state.market.instruments[0];valid_item.quote.last=191;valid->instruments.push_back(valid_item);
    app.handle_window_message(app.shell.window,WM_DATA_READY,static_cast<WPARAM>(*newer_request),reinterpret_cast<LPARAM>(valid.release()));
    check(!app.state.market.refreshing()&&IsWindowEnabled(GetDlgItem(app.shell.window,ID_REFRESH))&&app.state.market.instruments[0].quote.last==191,"only current request can apply and unlock refresh");
    // Real Win32 views consume domain results; no provider requests or account writes.
    app.trades_page.records={{"SBER",10,100,120,10,13},{"SBER",5,120,100,5,13}};
    app.fill_trades();
    check(app.narrow(app.read_edit(app.trades_page.summary)).find("60.30")!=std::string::npos,"closed trades display domain net total");
    auto& dividend_item=app.state.market.instruments[0];
    dividend_item.dividends_stale=false;
    DividendEvent dividend;dividend.record_date="2026-07-08";dividend.last_buy_date="2026-07-07";
    dividend.amount_per_share=10;dividend.currency="RUB";dividend.status=DividendStatus::Approved;
    dividend_item.dividends={dividend};
    app.portfolio_page.transactions={{"2026-07-01",dividend_item.ticker,PortfolioSide::Buy,10,100,0}};
    app.dividends_page.ticker=dividend_item.ticker;
    app.dividends_page.list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|LVS_REPORT,0,0,800,200,app.shell.window,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(app.dividends_page.list!=nullptr,"dividend result view created");
    for(int i=0;i<11;++i){LVCOLUMNW column{};column.mask=LVCF_WIDTH;column.cx=120;ListView_InsertColumn(app.dividends_page.list,i,&column);}
    app.fill_dividend_list();
    wchar_t expected_text[1024]{};
    ListView_GetItemText(app.dividends_page.list,0,10,expected_text,1024);
    check(app.narrow(expected_text).find("100.00 RUB")!=std::string::npos,"dividend view displays expected gross entitlement from domain");
    dividend_item.dividends_stale=true;app.fill_dividend_list();
    ListView_GetItemText(app.dividends_page.list,0,10,expected_text,1024);
    check(app.narrow(expected_text).starts_with("н/д:"),"stale event cannot display confirmed portfolio estimate");
    DestroyWindow(app.dividends_page.list);app.dividends_page.list=nullptr;
    Application first_owner,second_owner;
    first_owner.state.persistence.writes_blocked=true;second_owner.state.persistence.writes_blocked=true;
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=router_host;wc.lpszClassName=L"MOEXWatchRouterTest";
    check(RegisterClassExW(&wc)!=0,"router fixture class registered");
    first_owner.shell.window=CreateWindowExW(0,wc.lpszClassName,L"first",WS_OVERLAPPEDWINDOW,0,0,1180,760,nullptr,nullptr,wc.hInstance,&first_owner);
    second_owner.shell.window=CreateWindowExW(0,wc.lpszClassName,L"second",WS_OVERLAPPEDWINDOW,0,0,1180,760,nullptr,nullptr,wc.hInstance,&second_owner);
    check(first_owner.shell.window&&second_owner.shell.window,"instance-bound router windows created");
    check(GetPropW(first_owner.shell.window,L"MOEXWatch.Owner")==&first_owner&&GetPropW(second_owner.shell.window,L"MOEXWatch.Owner")==&second_owner,"window ownership stays instance-specific");
    first_owner.shell.tabs=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD,0,0,400,30,first_owner.shell.window,nullptr,wc.hInstance,nullptr);
    TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=const_cast<wchar_t*>(L"Test");TabCtrl_InsertItem(first_owner.shell.tabs,0,&tab);TabCtrl_InsertItem(first_owner.shell.tabs,1,&tab);TabCtrl_SetCurSel(first_owner.shell.tabs,1);
    first_owner.shell.active_page=4;second_owner.shell.active_page=4;
    NMHDR page_change{};page_change.hwndFrom=first_owner.shell.tabs;page_change.idFrom=ID_TABS;page_change.code=TCN_SELCHANGE;
    SendMessageW(first_owner.shell.window,WM_NOTIFY,ID_TABS,reinterpret_cast<LPARAM>(&page_change));
    check(first_owner.shell.active_page==1&&second_owner.shell.active_page==4,"native routing cannot change another application owner");
    DestroyWindow(first_owner.shell.window);DestroyWindow(second_owner.shell.window);
    check(GetPropW(first_owner.shell.window,L"MOEXWatch.Owner")==nullptr,"destroyed window releases owner binding");
    const auto no_owner=CreateWindowExW(0,wc.lpszClassName,L"invalid",WS_OVERLAPPEDWINDOW,0,0,1180,760,nullptr,nullptr,wc.hInstance,nullptr);
    check(no_owner==nullptr,"router rejects creation without an owner");
    std::cout<<"ui_pages_tests: all page, routing, layout and chart checks passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
