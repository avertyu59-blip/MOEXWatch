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
    std::cout<<"ui_pages_tests: backtest checks passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
