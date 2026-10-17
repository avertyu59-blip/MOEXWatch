#include "ui_app.hpp"
#ifdef _WIN32
#include <uxtheme.h>
namespace moexwatch::ui {
namespace {
RECT bounds(HWND child,HWND parent) {
    RECT r{};wchar_t type[64]{};GetClassNameW(child,type,64);
    if(lstrcmpiW(type,L"ComboBox")==0)SendMessageW(child,CB_GETDROPPEDCONTROLRECT,0,reinterpret_cast<LPARAM>(&r));
    else GetWindowRect(child,&r);
    MapWindowPoints(nullptr,parent,reinterpret_cast<POINT*>(&r),2);return r;
}
void fill(HDC dc,RECT r,COLORREF color) {const auto b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);}
void label(HDC dc,RECT r,const wchar_t* text,COLORREF color,UINT format=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS) {
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,text,-1,&r,format|DT_NOPREFIX);
}
LRESULT CALLBACK surface_proc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    auto* app=reinterpret_cast<Application*>(data);const auto c=theme::colors(app->shell.dark_theme);
    if(message==WM_ERASEBKGND && window==app->signals_page.observation_page) {
        RECT r{};GetClientRect(window,&r);FillRect(reinterpret_cast<HDC>(wp),&r,app->shell.surface_brush);return 1;
    }
    if(message==WM_DRAWITEM && app->draw_themed_item(*reinterpret_cast<DRAWITEMSTRUCT*>(lp)))return TRUE;
    if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORBTN||message==WM_CTLCOLORLISTBOX) {
        const auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,c.text);SetBkColor(dc,c.surface);
        return reinterpret_cast<LRESULT>(app->shell.surface_brush);
    }
    if(message==WM_NOTIFY) {
        const auto* hdr=reinterpret_cast<NMHDR*>(lp);wchar_t name[64]{};GetClassNameW(hdr->hwndFrom,name,64);
        if(hdr->code==NM_CUSTOMDRAW && lstrcmpiW(name,WC_HEADERW)==0) {
            const auto* cd=reinterpret_cast<NMCUSTOMDRAW*>(lp);
            if(cd->dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
            if(cd->dwDrawStage==CDDS_ITEMPREPAINT) {
                fill(cd->hdc,cd->rc,c.raised);wchar_t title[256]{};HDITEMW item{};item.mask=HDI_TEXT;item.pszText=title;item.cchTextMax=256;
                Header_GetItem(hdr->hwndFrom,static_cast<int>(cd->dwItemSpec),&item);
                auto r=cd->rc;r.left+=8;label(cd->hdc,r,title,c.muted);return CDRF_SKIPDEFAULT;
            }
        }
    }
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,surface_proc,id);
    return DefSubclassProc(window,message,wp,lp);
}
}
void Application::initialize_appearance() {
    if(!state.persistence.writes_blocked)storage_operation([&]{if(const auto value=app_storage.read_text("appearance.txt"))shell.dark_theme=*value!="light";});
    shell.heading_font=CreateFontW(-21,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    shell.metric_font=CreateFontW(-29,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    // Retain the tab model for existing page commands; visible navigation uses native buttons.
    ShowWindow(shell.tabs,SW_HIDE);
    for(HWND child=GetWindow(shell.window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
        if(child!=shell.tabs)shell.base_layout.emplace_back(child,bounds(child,shell.window));
    }
    const wchar_t* names[]={L"Обзор и графики",L"Закрытые сделки",L"Портфель",L"Сигналы",L"Тест стратегии",L"История сигналов",L"Дивиденды",L"Сменить тему"};
    for(int i=0;i<8;++i) {
        shell.navigation[i]=CreateWindowExW(0,L"BUTTON",names[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            12,80+i*48,theme::sidebar-24,40,shell.window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(theme::navigation_first+i)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(shell.navigation[i],WM_SETFONT,reinterpret_cast<WPARAM>(shell.font),TRUE);
    }
    apply_appearance();
}
void Application::apply_appearance() {
    const auto c=theme::colors(shell.dark_theme);
    if(shell.canvas_brush)DeleteObject(shell.canvas_brush);if(shell.surface_brush)DeleteObject(shell.surface_brush);
    shell.canvas_brush=CreateSolidBrush(c.canvas);shell.surface_brush=CreateSolidBrush(c.surface);
    EnumChildWindows(shell.window,[](HWND h,LPARAM data)->BOOL {
        auto* app=reinterpret_cast<Application*>(data);const auto colors=theme::colors(app->shell.dark_theme);
        wchar_t type[64]{};GetClassNameW(h,type,64);
        SetWindowSubclass(h,surface_proc,1,static_cast<DWORD_PTR>(data));
        if(lstrcmpiW(type,WC_LISTVIEWW)==0) {
            ListView_SetBkColor(h,colors.surface);ListView_SetTextBkColor(h,colors.surface);ListView_SetTextColor(h,colors.text);
            ListView_SetExtendedListViewStyleEx(h,LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER,LVS_EX_DOUBLEBUFFER);
            SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(app->shell.font),TRUE);
        }
        if(lstrcmpiW(type,L"Button")==0) {
            auto style=GetWindowLongPtrW(h,GWL_STYLE);const auto kind=style&BS_TYPEMASK;
            if(kind==BS_PUSHBUTTON||kind==BS_DEFPUSHBUTTON)SetWindowLongPtrW(h,GWL_STYLE,(style&~BS_TYPEMASK)|BS_OWNERDRAW|WS_TABSTOP);
            SetWindowTheme(h,L"",L"");
        }
        if(lstrcmpiW(type,L"Edit")==0||lstrcmpiW(type,L"ComboBox")==0||lstrcmpiW(type,WC_LISTVIEWW)==0)
            SetWindowLongPtrW(h,GWL_STYLE,GetWindowLongPtrW(h,GWL_STYLE)|WS_TABSTOP);
        if(lstrcmpiW(type,WC_TABCONTROLW)==0) {
            SetWindowTheme(h,L"",L"");SetWindowLongPtrW(h,GWL_STYLE,GetWindowLongPtrW(h,GWL_STYLE)|TCS_OWNERDRAWFIXED);
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(this));
    RedrawWindow(shell.window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
void Application::layout_workspace(int width,int height) {
    if(shell.base_layout.empty())return;
    const int available=std::max(1164,width-theme::sidebar);
    for(const auto& [control,r]:shell.base_layout)MoveWindow(control,r.left,r.top,r.right-r.left,r.bottom-r.top,FALSE);
    resize_charts(available,height);resize_trades(available,height);resize_signals(available,height);
    resize_backtest(available,height);resize_portfolio(available,height-112);
    MoveWindow(portfolio_page.positions,16,185,available-360,220,FALSE);
    if(signals_page.observation_page)MoveWindow(signals_page.observation_page,16,64,available-32,height-80,FALSE);
    for(const auto& [control,initial]:shell.base_layout) {
        (void)initial;auto r=bounds(control,shell.window);
        const bool portfolio=std::find(portfolio_page.controls.begin(),portfolio_page.controls.end(),control)!=portfolio_page.controls.end();
        const int shift=portfolio?112:0;
        MoveWindow(control,r.left+theme::sidebar,r.top+shift,std::min(static_cast<int>(r.right-r.left),std::max(40,available-static_cast<int>(r.left)-16)),r.bottom-r.top,FALSE);
    }
    // Global actions have a dedicated header and sidebar; never cover page editors.
    MoveWindow(GetDlgItem(shell.window,ID_REFRESH),width-148,12,132,32,FALSE);
    MoveWindow(shell.cancel_loading,available>1200?width-496:width-480,12,160,32,FALSE);
    MoveWindow(shell.diagnostics,width-310,12,150,32,FALSE);
    MoveWindow(shell.backup_button,12,height-100,theme::sidebar-24,36,FALSE);
    MoveWindow(shell.navigation[7],12,height-148,theme::sidebar-24,36,FALSE);
    InvalidateRect(shell.window,nullptr,TRUE);
}
bool Application::draw_themed_item(const DRAWITEMSTRUCT& item) {
    if(item.CtlType!=ODT_BUTTON && item.CtlType!=ODT_TAB)return false;
    const auto c=theme::colors(shell.dark_theme);const int id=GetDlgCtrlID(item.hwndItem);
    const bool nav=id>=theme::navigation_first&&id<theme::navigation_first+8;
    const bool selected=nav && id-theme::navigation_first==shell.active_page;
    const bool primary=id==ID_REFRESH||id==ID_PORT_ADD||id==ID_SIG_SAVE||id==ID_BACKTEST_RUN;
    const bool disabled=(item.itemState&ODS_DISABLED)!=0;
    const int saved=SaveDC(item.hDC);SelectObject(item.hDC,shell.font);
    fill(item.hDC,item.rcItem,selected?c.selected:((item.itemState&ODS_SELECTED)?c.border:(primary?c.accent:c.raised)));
    wchar_t text[256]{};
    if(item.CtlType==ODT_TAB) {TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=text;tab.cchTextMax=256;TabCtrl_GetItem(item.hwndItem,item.itemID,&tab);}
    else GetWindowTextW(item.hwndItem,text,256);
    auto r=item.rcItem;r.left+=10;r.right-=8;
    label(item.hDC,r,text,disabled?c.muted:(primary?RGB(255,255,255):c.text),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|(nav?DT_LEFT:DT_CENTER));
    if(selected){auto marker=item.rcItem;marker.right=marker.left+3;fill(item.hDC,marker,c.accent);}
    if(item.itemState&ODS_FOCUS){auto focus=item.rcItem;InflateRect(&focus,-3,-3);DrawFocusRect(item.hDC,&focus);}
    RestoreDC(item.hDC,saved);return true;
}
void Application::paint_workspace(HDC dc,RECT client) {
    const auto c=theme::colors(shell.dark_theme);const int saved=SaveDC(dc);
    RECT side{0,0,theme::sidebar,client.bottom};fill(dc,side,c.surface);
    SelectObject(dc,shell.heading_font);RECT logo{16,16,theme::sidebar-8,56};label(dc,logo,L"MOEX Watch",c.text);
    const wchar_t* titles[]={L"Обзор и графики",L"Закрытые сделки",L"Портфель",L"Сигналы",L"Тест стратегии",L"История сигналов"};
    RECT title{theme::sidebar+16,8,client.right-500,48};label(dc,title,titles[shell.active_page],c.text);
    SelectObject(dc,shell.font);RECT source{16,client.bottom-54,theme::sidebar-8,client.bottom-12};
    label(dc,source,L"Источник: MOEX ISS",c.muted);
    RestoreDC(dc,saved);
}
void Application::paint_portfolio_cards(HDC dc,int width) {
    const auto c=theme::colors(shell.dark_theme);const int saved=SaveDC(dc);
    const auto portfolio=calculate_portfolio(portfolio_page.transactions,portfolio_quotes());
    const bool complete=portfolio && portfolio->priced_positions==portfolio->open_positions;
    bool fresh=complete;
    if(portfolio)for(const auto& position:portfolio->positions)if(position.quantity>0) {
        const auto index=state.market.index_of(position.ticker);if(!index||state.market.instruments[*index].quote_stale)fresh=false;
    }
    const wchar_t* titles[]={L"Стоимость позиций",L"Бумажная прибыль",L"Зафиксировано"};
    for(int i=0;i<3;++i) {
        const int card=(width-64)/3;RECT r{16+i*(card+16),56,16+i*(card+16)+card,152};fill(dc,r,c.surface);
        SelectObject(dc,shell.font);RECT title{r.left+16,r.top+8,r.right-12,r.top+32};label(dc,title,titles[i],c.muted);
        const double value=!portfolio?0:(i==0?portfolio->total_market_value:i==1?portfolio->total_unrealized_profit:portfolio->total_realized_profit);
        const bool missing=!portfolio||(i<2 && portfolio->open_positions>0 && portfolio->priced_positions==0);
        const auto text=missing?std::wstring(L"н/д"):(i>0 && value>0?std::wstring(L"+"):std::wstring{})+widen(money(value))+L" ₽";
        SelectObject(dc,shell.metric_font);RECT amount{r.left+16,r.top+32,r.right-12,r.top+70};
        label(dc,amount,text.c_str(),i==0||!portfolio?c.text:(value<0?c.negative:c.positive));
        SelectObject(dc,shell.font);RECT status{r.left+16,r.top+72,r.right-12,r.bottom-4};
        label(dc,status,!portfolio?L"Ошибка журнала":(i==2?L"По журналу операций":(fresh?L"По доступным котировкам":L"Неполная / устаревшая оценка")),fresh||i==2?c.muted:c.warning);
    }
    RECT allocation{width-328,297,width-16,517};fill(dc,allocation,c.surface);
    RECT caption{allocation.left+12,allocation.top+8,allocation.right-12,allocation.top+32};
    SelectObject(dc,shell.font);label(dc,caption,L"Структура позиций",c.text);
    if(!fresh || !portfolio || portfolio->total_market_value<=0) {
        RECT notice{allocation.left+12,allocation.top+48,allocation.right-12,allocation.bottom-12};
        label(dc,notice,L"Для долей нужны актуальные котировки всех позиций",c.muted,DT_LEFT|DT_WORDBREAK);
    } else {
        std::vector<std::pair<std::string,double>> weights;
        for(const auto& position:portfolio->positions)if(position.quantity>0 && position.market_value)
            weights.emplace_back(position.ticker,*position.market_value/portfolio->total_market_value);
        std::sort(weights.begin(),weights.end(),[](const auto& a,const auto& b){return a.second>b.second;});
        if(weights.size()>4){double other=0;for(std::size_t i=4;i<weights.size();++i)other+=weights[i].second;weights.resize(4);weights.emplace_back("Прочие",other);}
        const COLORREF colors[]={c.positive,c.accent,c.purple,c.warning,c.muted};
        const int cx=allocation.left+78,cy=allocation.top+120,radius=62;
        double start=-1.5707963267948966;
        for(std::size_t i=0;i<weights.size();++i) {
            const double end=start+weights[i].second*6.283185307179586;
            std::vector<POINT> points{{cx,cy}};
            for(int step=0;step<=64;++step){const double angle=start+(end-start)*step/64;points.push_back({cx+static_cast<LONG>(radius*std::cos(angle)),cy+static_cast<LONG>(radius*std::sin(angle))});}
            auto brush=CreateSolidBrush(colors[i]);auto old=SelectObject(dc,brush);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));
            Polygon(dc,points.data(),static_cast<int>(points.size()));SelectObject(dc,old);SelectObject(dc,pen);DeleteObject(brush);
            RECT legend{allocation.left+152,allocation.top+44+static_cast<LONG>(i)*28,allocation.right-8,allocation.top+72+static_cast<LONG>(i)*28};
            label(dc,legend,widen(weights[i].first+"  "+money(weights[i].second*100)+"%").c_str(),colors[i]);start=end;
        }
        auto hole=CreateSolidBrush(c.surface);auto old=SelectObject(dc,hole);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));
        Ellipse(dc,cx-38,cy-38,cx+38,cy+38);SelectObject(dc,old);SelectObject(dc,pen);DeleteObject(hole);
    }
    RestoreDC(dc,saved);
}
}
#endif
