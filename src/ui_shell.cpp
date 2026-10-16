#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {

void Application::storage_failure(const char* error) {
    state.persistence.writes_blocked = true;
    try{if(!log)(void)fetch_context(0);if(log)log->record(0,"storage","writes_blocked");}catch(...){ }
    MessageBoxW(shell.window, widen(std::string("Ошибка хранения данных: ") + error +
        "\nСделайте резервную копию. До исправления ошибки автоматическая запись отключена.").c_str(), L"MOEXWatch", MB_ICONERROR);
}

void Application::refresh_data() {
    const auto request=state.market.begin_refresh();
    if(!request)return;
    EnableWindow(GetDlgItem(shell.window, ID_REFRESH), FALSE);
    SetWindowTextW(charts_page.status, L"Загружаю акции и историю IMOEX из MOEX ISS…");
    const auto source = state.market.instruments;
    const auto completions=mailbox;
    try {auto context=fetch_context(*request);tasks.start(TaskKind::Market,mailbox,[source,context,completions](std::stop_token stop) mutable -> BackgroundEvent {
        context.stop=stop;MarketRefreshResult result;
        try {
            completions->publish({context.request,FetchProgress{0,source.size()}});
            try{result.imoex_candles=fetch_index_candles(context);}catch(const Cancelled&){throw;}catch(const std::exception& ex){result.imoex_error=ex.what();}
            for(const auto& item:source){
                check_cancelled(stop);completions->publish({context.request,FetchProgress{result.instruments.size()+1,source.size()}});
                result.instruments.push_back(fetch_instrument(item,context));
            }
            check_cancelled(stop);
        }catch(const Cancelled&){result=MarketRefreshResult{};result.cancelled=true;}
        return {context.request,std::move(result)};
    });}catch(const std::exception&){state.market.cancel_refresh();EnableWindow(GetDlgItem(shell.window,ID_REFRESH),TRUE);SetWindowTextW(charts_page.status,L"Не удалось запустить загрузку.");}
    update_task_controls();
}

void Application::refresh_market_views() {
    if(dividends_page.window){fill_dividend_list();InvalidateRect(dividends_page.window,nullptr,TRUE);}
    fill_list();fill_signals();fill_portfolio();notify_new_signals();
}
void Application::apply_market_refresh(std::uint64_t request,MarketRefreshResult result) {
    if(!state.market.is_current(request))return;
    if(result.cancelled){state.market.cancel_refresh();EnableWindow(GetDlgItem(shell.window,ID_REFRESH),TRUE);SetWindowTextW(charts_page.status,L"Загрузка отменена. Сохранены предыдущие данные.");update_task_controls();return;}
    if(log)log->record(request,"market","apply_begin");
    const auto applied=state.market.apply_refresh(request,std::move(result));
    if(applied==MarketState::ApplyResult::Ignored)return;
    EnableWindow(GetDlgItem(shell.window,ID_REFRESH),TRUE);
    if(applied==MarketState::ApplyResult::RosterChanged){refresh_data();return;}
    const auto loaded = std::count_if(state.market.instruments.begin(), state.market.instruments.end(), [](const auto& item) { return item.quote.last && !item.quote_stale; });
    std::wstring status = L"Котировки загружены: " + std::to_wstring(loaded) + L" из " + std::to_wstring(state.market.instruments.size()) + L". ";
    status += state.market.imoex_stale ? L"IMOEX не обновлён; сравнение использует старые данные. " : L"IMOEX обновлён. ";
    status += L"Подробности — в выбранной акции.";
    SetWindowTextW(charts_page.status, status.c_str());
    refresh_market_views();
    if(log)log->record(request,"market","apply_ok");
    update_task_controls();
    if(state.market.take_pending_refresh())refresh_data();
}

void Application::activate_page(int page) {
    shell.active_page = std::clamp(page, 0, 5);
    if(signals_page.observation_page){ShowWindow(signals_page.observation_page,shell.active_page==5?SW_SHOW:SW_HIDE);if(shell.active_page==5)refresh_observation_page(signals_page.observation_page);}
    for (const auto control : charts_page.market_controls) ShowWindow(control, shell.active_page == 0 ? SW_SHOW : SW_HIDE);
    for (const auto control : trades_page.controls) ShowWindow(control, shell.active_page == 1 ? SW_SHOW : SW_HIDE);
    for (const auto control : portfolio_page.controls) ShowWindow(control, shell.active_page == 2 ? SW_SHOW : SW_HIDE);
    for (const auto control : signals_page.controls) ShowWindow(control, shell.active_page == 3 ? SW_SHOW : SW_HIDE);
    for (const auto control : backtest_page.controls) ShowWindow(control, shell.active_page == 4 ? SW_SHOW : SW_HIDE);
    for (const auto control : shell.global_controls) ShowWindow(control, SW_SHOW);
    InvalidateRect(shell.window, nullptr, TRUE);
}

void Application::start_backup() {
    if (!IsWindowEnabled(shell.backup_button)) return;
    // Flush the UI-owned state before the worker takes a consistent disk snapshot.
    // If loading failed, preserve the original files for recovery instead.
    if (!state.persistence.writes_blocked) { save_watchlist(); save_trades(); save_portfolio(); save_signal_settings(); }
    EnableWindow(shell.backup_button, FALSE);
    try {const auto root=storage::data_directory();
    tasks.start(TaskKind::Backup,mailbox,[root](std::stop_token stop)->BackgroundEvent {
        BackupResult result;try{result.path=storage::create_backup(root,stop);}catch(const Cancelled&){result.cancelled=true;}catch(const std::exception& ex){result.error=ex.what();}
        return {0,std::move(result)};
    });}catch(const std::exception& ex){EnableWindow(shell.backup_button,TRUE);MessageBoxW(shell.window,widen(ex.what()).c_str(),L"Резервная копия",MB_ICONERROR);}
}

void Application::create_controls(HWND window) {
    shell.font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    shell.tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                            16, 8, 400, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TABS)), GetModuleHandleW(nullptr), nullptr);
    shell.backup_button=CreateWindowExW(0,L"BUTTON",L"Резервная копия",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        1150,9,155,28,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BACKUP)),GetModuleHandleW(nullptr),nullptr);
    shell.cancel_loading=CreateWindowExW(0,L"BUTTON",L"Отменить загрузки",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,800,9,155,28,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CANCEL_LOADING)),GetModuleHandleW(nullptr),nullptr);
    shell.diagnostics=CreateWindowExW(0,L"BUTTON",L"Журнал ошибок",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,965,9,140,28,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIAGNOSTICS)),GetModuleHandleW(nullptr),nullptr);
    for(const auto h:{shell.cancel_loading,shell.diagnostics}){if(shell.font)SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(shell.font),TRUE);shell.global_controls.push_back(h);}
    EnableWindow(shell.cancel_loading,FALSE);
    if(shell.font)SendMessageW(shell.backup_button,WM_SETFONT,reinterpret_cast<WPARAM>(shell.font),TRUE);
    TCITEMW tab{}; tab.mask = TCIF_TEXT;
    tab.pszText = const_cast<wchar_t*>(L"Аналитика"); TabCtrl_InsertItem(shell.tabs, 0, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Закрытые сделки"); TabCtrl_InsertItem(shell.tabs, 1, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Портфель"); TabCtrl_InsertItem(shell.tabs, 2, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Сигналы"); TabCtrl_InsertItem(shell.tabs, 3, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Тест стратегии"); TabCtrl_InsertItem(shell.tabs, 4, &tab);
    tab.pszText = const_cast<wchar_t*>(L"История сигналов"); TabCtrl_InsertItem(shell.tabs, 5, &tab);
    TabCtrl_SetCurSel(shell.tabs, 0);
    if (shell.font) SendMessageW(shell.tabs, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);

    create_charts_controls(window);
    create_trades_controls(window);
    create_portfolio_controls(window);
    create_signals_controls(window);
    create_backtest_controls(window);
    create_observation_controls(window);
    activate_page(0);
    SetTimer(window,TIMER_TASKS,50,nullptr);
}

LRESULT CALLBACK Application::handle_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        shell.window = window; create_controls(window); load_signal_settings(); synchronize_signal_controls();
        update_auto_refresh_timer(); load_watchlist(); load_observations(); load_trades(); load_portfolio(); load_market_cache(state.market.instruments, state.market.imoex_candles, state.market.imoex_error); fill_list(); fill_trades(); fill_signals(); fill_portfolio();
        for (const auto& item : state.market.instruments) {
            SendMessageW(backtest_page.ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(item.ticker).c_str()));
            SendMessageW(portfolio_page.ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(item.ticker).c_str()));
        }
        if (!state.market.instruments.empty()) { SendMessageW(backtest_page.ticker, CB_SETCURSEL, 0, 0); SendMessageW(portfolio_page.ticker, CB_SETCURSEL, 0, 0); }
        SetWindowTextW(portfolio_page.date, widen(today_date()).c_str());
        add_notification_icon(); refresh_data(); return 0;
    case WM_SIZE: {
        const int width = LOWORD(lparam), height = HIWORD(lparam);
        if(signals_page.observation_page)MoveWindow(signals_page.observation_page,16,100,width-32,height-116,TRUE);
        if (shell.tabs) MoveWindow(shell.tabs,16,8,std::max(400,width-526),34,TRUE);
        if(shell.cancel_loading)MoveWindow(shell.cancel_loading,width-496,9,155,28,TRUE);
        if(shell.diagnostics)MoveWindow(shell.diagnostics,width-331,9,140,28,TRUE);
        if(shell.backup_button)MoveWindow(shell.backup_button, width-171, 9, 155, 28, TRUE);
        resize_charts(width, height);
        resize_trades(width, height);
        resize_signals(width, height);
        resize_backtest(width, height);
        resize_portfolio(width, height);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize.x = 1180;
        limits->ptMinTrackSize.y = 760;
        return 0;
    }
    case WM_COMMAND:
        if(command_charts(wparam)||command_backtest(wparam)||command_signals(wparam)||command_portfolio(wparam)||command_trades(wparam))return 0;
        switch (LOWORD(wparam)) {
        case ID_CANCEL_LOADING: cancel_loading(); return 0;
        case ID_DIAGNOSTICS: open_diagnostics(); return 0;
        case ID_REFRESH: refresh_data(); return 0;
        case ID_BACKUP: start_backup(); return 0;
        }
        break;
    case WM_NOTIFY:
        if(notify_charts(lparam))return 0;
        if(reinterpret_cast<NMHDR*>(lparam)->code==LVN_COLUMNCLICK) {
            const auto* click=reinterpret_cast<NMLISTVIEW*>(lparam);const int id=static_cast<int>(click->hdr.idFrom);
            if(id==ID_LIST||id==ID_TRADE_LIST||id==ID_PORT_POSITIONS||id==ID_PORT_TRANSACTIONS||id==ID_SIG_LIST){auto& sort=*table_sort(id);if(sort.column==click->iSubItem)sort.ascending=!sort.ascending;else{sort.column=click->iSubItem;sort.ascending=true;}apply_table_sort(click->hdr.hwndFrom,id);if(id==ID_PORT_POSITIONS)fill_position_level_editor();return 0;}
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_TABS && reinterpret_cast<NMHDR*>(lparam)->code == TCN_SELCHANGE) {
            activate_page(TabCtrl_GetCurSel(shell.tabs)); return 0;
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_PORT_POSITIONS &&
            reinterpret_cast<NMHDR*>(lparam)->code == LVN_ITEMCHANGED) {
            if(portfolio_page.updating)return 0;
            const auto* changed = reinterpret_cast<const NMLISTVIEW*>(lparam);
            if ((changed->uNewState & LVIS_SELECTED) != 0) fill_position_level_editor();
            return 0;
        }
        return 0;
    case WM_FETCH_PROGRESS: {
        const std::wstring status = wparam == 0
            ? std::wstring(L"Загрузка дневной истории IMOEX…")
            : std::wstring(L"Загрузка котировок и истории: акция ") + std::to_wstring(wparam) + L" из " + std::to_wstring(lparam);
        SetWindowTextW(charts_page.status, status.c_str()); return 0;
    }
    case WM_TIMER:
        if(wparam==TIMER_TASKS){drain_completions();return 0;}
        if (wparam == TIMER_AUTO_REFRESH && signals_page.settings.auto_refresh_enabled) { refresh_data(); return 0; }
        break;
    case WM_DATA_READY: {
        std::unique_ptr<MarketRefreshResult> data(reinterpret_cast<MarketRefreshResult*>(lparam));
        if(data)apply_market_refresh(static_cast<std::uint64_t>(wparam),std::move(*data));
        return 0;
    }
    case WM_BACKUP_READY: {
        std::unique_ptr<BackupResult> result(reinterpret_cast<BackupResult*>(lparam));
        EnableWindow(shell.backup_button,TRUE);
        if(result->cancelled)return 0;
        if(!result->error.empty())MessageBoxW(window,widen("Резервная копия не создана: "+result->error).c_str(),L"MOEXWatch",MB_ICONERROR);
        else {
            MessageBoxW(window,(L"Копия создана и проверена:\n"+result->path.wstring()+
                L"\n\nДля восстановления закройте приложение и следуйте RESTORE.txt в этой папке.").c_str(),L"Резервная копия",MB_ICONINFORMATION);
            ShellExecuteW(window,L"open",result->path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        }
        return 0;
    }
    case WM_DIVIDENDS_READY: {
        std::unique_ptr<DividendFetchResult> result(reinterpret_cast<DividendFetchResult*>(lparam));
        if(result)apply_dividend_response(static_cast<std::uint64_t>(wparam),std::move(*result));
        return 0;
    }
    case WM_BACKTEST_READY: {
        complete_backtest(lparam);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(window, &ps);
        if (shell.active_page == 0) {
            RECT client{}; GetClientRect(window, &client);
            paint_charts(dc,client);
        }
        EndPaint(window, &ps); return 0;
    }
    case WM_CLOSE:
        shutdown_tasks();DestroyWindow(window);return 0;
    case WM_DESTROY:
        shutdown_tasks();
        KillTimer(window,TIMER_TASKS);
        KillTimer(window, TIMER_AUTO_REFRESH);
        save_watchlist(); save_trades(); save_portfolio(); save_signal_settings();
        if (signals_page.notification_icon_ready) Shell_NotifyIconW(NIM_DELETE, &signals_page.notification_icon);
        if (shell.font) DeleteObject(shell.font); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace moexwatch::ui

namespace moexwatch::ui {
namespace {
Application* owner(HWND window,UINT message,LPARAM lp) {
    if(message==WM_NCCREATE) {
        auto* app=static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        if(!app || !SetPropW(window,L"MOEXWatch.Owner",app))return nullptr;
    }
    return static_cast<Application*>(GetPropW(window,L"MOEXWatch.Owner"));
}
}
LRESULT CALLBACK Application::window_proc(HWND w,UINT m,WPARAM p,LPARAM l) {
    auto* app=owner(w,m,l);
    if(m==WM_NCCREATE&&!app)return FALSE;
    const auto result=app?app->handle_window_message(w,m,p,l):DefWindowProcW(w,m,p,l);
    if(m==WM_NCDESTROY)RemovePropW(w,L"MOEXWatch.Owner");
    return result;
}
LRESULT CALLBACK Application::dividends_window_proc(HWND w,UINT m,WPARAM p,LPARAM l) {
    auto* app=owner(w,m,l);
    if(m==WM_NCCREATE&&!app)return FALSE;
    const auto result=app?app->handle_dividends_message(w,m,p,l):DefWindowProcW(w,m,p,l);
    if(m==WM_NCDESTROY)RemovePropW(w,L"MOEXWatch.Owner");
    return result;
}
}

#endif
