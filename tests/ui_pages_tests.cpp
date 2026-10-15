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
        app.app_window=CreateWindowExW(0,L"STATIC",L"MOEXWatch page tests",WS_OVERLAPPEDWINDOW,0,0,1340,870,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        check(app.app_window!=nullptr,"hidden owner created");
        app.app_storage_blocked=true; // No real account files, network, or notifications in this fixture.
        app.create_controls(app.app_window);
    }
    ~Fixture(){KillTimer(app.app_window,TIMER_AUTO_REFRESH);DestroyWindow(app.app_window);if(app.app_font)DeleteObject(app.app_font);}
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
    // Real chart painting into a DIB exercises every GDI path without network data.
    app.app_items[0].candles.clear();app.app_imoex_candles.clear();
    using namespace std::chrono;
    const auto last_session=sys_days{year{2026}/10/15};
    for(int i=0;i<270;++i){
        const auto date=observation_time(duration_cast<seconds>((last_session-days{269-i}).time_since_epoch()).count()).substr(0,10);
        const double price=100.0+i*0.3+std::sin(i*0.3)*3;
        app.app_items[0].candles.push_back({date,price-1,price+2,price-2,price,i==269?5000.0:1000.0});
        const double index=3000.0+i*2;
        app.app_imoex_candles.push_back({date,index-1,index+2,index-2,index,10000});
    }
    app.app_imoex_stale=false;
    Instrument second=app.app_items[0];second.ticker="GAZP";second.quote.ticker="GAZP";second.quote.last=90;second.target=95;
    app.app_items.push_back(second);
    app.app_table_sorts[ID_LIST]={1,true};app.fill_list();
    check(row_data(app.charts_page.list,0)==1,"watch table sorts without changing instrument identity");
    ListView_SetItemState(app.charts_page.list,0,LVIS_SELECTED,LVIS_SELECTED);
    NMLISTVIEW selected{};selected.hdr.hwndFrom=app.charts_page.list;selected.hdr.idFrom=ID_LIST;selected.hdr.code=LVN_ITEMCHANGED;
    app.handle_window_message(app.app_window,WM_NOTIFY,ID_LIST,reinterpret_cast<LPARAM>(&selected));
    check(app.charts_page.selected==1,"main coordinator forwards selected stock to analytics");
    SetWindowTextW(app.charts_page.target_edit,L"150");
    app.handle_window_message(app.app_window,WM_COMMAND,MAKEWPARAM(ID_TARGET_SET,0),0);
    check(app.app_items[1].target==150&&app.app_items[0].target==200,"target edit applies to selected stock after sorting");
    for(int range=0;range<4;++range){
        const int sessions[]={22,66,252,0};
        SendMessageW(GetDlgItem(app.app_window,ID_RANGE),CB_SETCURSEL,static_cast<WPARAM>(range),0);
        app.handle_window_message(app.app_window,WM_COMMAND,MAKEWPARAM(ID_RANGE,CBN_SELCHANGE),0);
        check(app.charts_page.chart_sessions==sessions[range],"range command reaches chart module");
    }
    SendMessageW(GetDlgItem(app.app_window,ID_FILTER),CB_SETCURSEL,4,0);
    app.handle_window_message(app.app_window,WM_COMMAND,MAKEWPARAM(ID_FILTER,CBN_SELCHANGE),0);
    check(ListView_GetItemCount(app.charts_page.list)==0&&app.app_items.size()==2,"empty filter does not remove watched stocks");
    SendMessageW(GetDlgItem(app.app_window,ID_FILTER),CB_SETCURSEL,0,0);
    app.handle_window_message(app.app_window,WM_COMMAND,MAKEWPARAM(ID_FILTER,CBN_SELCHANGE),0);
    check(ListView_GetItemCount(app.charts_page.list)==2,"all-stocks filter restores both rows");
    HDC dc=CreateCompatibleDC(nullptr);check(dc!=nullptr,"chart memory DC created");
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=800;bitmap.bmiHeader.biHeight=-420;bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;bitmap.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP surface=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&pixels,nullptr,0);
    check(surface!=nullptr&&pixels!=nullptr,"chart DIB created");
    const auto old_bitmap=SelectObject(dc,surface);const auto old_font=SelectObject(dc,app.app_font);
    const auto operations_before=export_portfolio_csv(app.portfolio_page.transactions);
    const auto paint=[&](int panel,int sessions){
        TabCtrl_SetCurSel(app.charts_page.chart_tabs,panel);
        NMHDR changed{};changed.hwndFrom=app.charts_page.chart_tabs;changed.idFrom=ID_CHART_TABS;changed.code=TCN_SELCHANGE;
        app.handle_window_message(app.app_window,WM_NOTIFY,ID_CHART_TABS,reinterpret_cast<LPARAM>(&changed));
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
    std::set<HWND> tracked{app.app_tabs,app.app_backup_button,app.signals_page.observation_page};
    for(const auto* page:pages)tracked.insert(page->begin(),page->end());
    tracked.insert(app.app_global_controls.begin(),app.app_global_controls.end());
    for(HWND child=GetWindow(app.app_window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))check(tracked.contains(child),"every root control has an explicit owner");
    for(int page=0;page<6;++page){
        app.activate_page(page);
        for(std::size_t group=0;group<pages.size();++group)for(HWND control:*pages[group])check(((GetWindowLongPtrW(control,GWL_STYLE)&WS_VISIBLE)!=0)==(static_cast<int>(group)==page),"switching page shows only its controls");
        check(((GetWindowLongPtrW(app.signals_page.observation_page,GWL_STYLE)&WS_VISIBLE)!=0)==(page==5),"history visibility is coordinated");
        check((GetWindowLongPtrW(GetDlgItem(app.app_window,ID_REFRESH),GWL_STYLE)&WS_VISIBLE)!=0,"refresh stays visible on every page");
        app.handle_window_message(app.app_window,WM_SIZE,0,MAKELPARAM(1164,720));
    }
    check(!app.command_charts(MAKEWPARAM(ID_BACKTEST_RUN,0)),"charts do not consume backtest command");
    // A refresh is applied by ticker while another page is active; the user target wins.
    app.activate_page(2);app.app_busy=true;EnableWindow(GetDlgItem(app.app_window,ID_REFRESH),FALSE);
    auto incoming=std::make_unique<MarketRefreshResult>();
    auto refreshed=app.app_items[0];refreshed.quote.last=190;refreshed.target=999;
    incoming->instruments.push_back(refreshed);incoming->imoex_candles=app.app_imoex_candles;
    app.handle_window_message(app.app_window,WM_DATA_READY,0,reinterpret_cast<LPARAM>(incoming.release()));
    check(!app.app_busy&&IsWindowEnabled(GetDlgItem(app.app_window,ID_REFRESH)),"refresh completion restores global button state");
    check(app.app_items[0].quote.last==190&&app.app_items[0].target==200,"refresh updates quote but preserves user target");
    check(app.app_active_page==2&&ListView_GetItemCount(app.portfolio_page.transactions_list)==2,"refresh updates views without changing active page or operations");
    Application first_owner,second_owner;
    first_owner.app_storage_blocked=true;second_owner.app_storage_blocked=true;
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=router_host;wc.lpszClassName=L"MOEXWatchRouterTest";
    check(RegisterClassExW(&wc)!=0,"router fixture class registered");
    first_owner.app_window=CreateWindowExW(0,wc.lpszClassName,L"first",WS_OVERLAPPEDWINDOW,0,0,1180,760,nullptr,nullptr,wc.hInstance,&first_owner);
    second_owner.app_window=CreateWindowExW(0,wc.lpszClassName,L"second",WS_OVERLAPPEDWINDOW,0,0,1180,760,nullptr,nullptr,wc.hInstance,&second_owner);
    check(first_owner.app_window&&second_owner.app_window,"instance-bound router windows created");
    check(GetPropW(first_owner.app_window,L"MOEXWatch.Owner")==&first_owner&&GetPropW(second_owner.app_window,L"MOEXWatch.Owner")==&second_owner,"window ownership stays instance-specific");
    first_owner.app_tabs=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD,0,0,400,30,first_owner.app_window,nullptr,wc.hInstance,nullptr);
    TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=const_cast<wchar_t*>(L"Test");TabCtrl_InsertItem(first_owner.app_tabs,0,&tab);TabCtrl_InsertItem(first_owner.app_tabs,1,&tab);TabCtrl_SetCurSel(first_owner.app_tabs,1);
    first_owner.app_active_page=4;second_owner.app_active_page=4;
    NMHDR page_change{};page_change.hwndFrom=first_owner.app_tabs;page_change.idFrom=ID_TABS;page_change.code=TCN_SELCHANGE;
    SendMessageW(first_owner.app_window,WM_NOTIFY,ID_TABS,reinterpret_cast<LPARAM>(&page_change));
    check(first_owner.app_active_page==1&&second_owner.app_active_page==4,"native routing cannot change another application owner");
    DestroyWindow(first_owner.app_window);DestroyWindow(second_owner.app_window);
    check(GetPropW(first_owner.app_window,L"MOEXWatch.Owner")==nullptr,"destroyed window releases owner binding");
    const auto no_owner=CreateWindowExW(0,wc.lpszClassName,L"invalid",WS_OVERLAPPEDWINDOW,0,0,1180,760,nullptr,nullptr,wc.hInstance,nullptr);
    check(no_owner==nullptr,"router rejects creation without an owner");
    std::cout<<"ui_pages_tests: all page, routing, layout and chart checks passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
