#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {

void Application::storage_failure(const char* error) {
    app_storage_blocked = true;
    MessageBoxW(app_window, widen(std::string("Ошибка хранения данных: ") + error +
        "\nСделайте резервную копию. До исправления ошибки автоматическая запись отключена.").c_str(), L"MOEXWatch", MB_ICONERROR);
}

void Application::refresh_data() {
    bool expected = false;
    if (!app_busy.compare_exchange_strong(expected, true)) return;
    EnableWindow(GetDlgItem(app_window, ID_REFRESH), FALSE);
    SetWindowTextW(charts_page.status, L"Загружаю акции и историю IMOEX из MOEX ISS…");
    const auto source = app_items;
    const HWND target = app_window;
    std::thread([source, target]() {
        auto result = std::make_unique<MarketRefreshResult>();
        PostMessageW(target, WM_FETCH_PROGRESS, 0, static_cast<LPARAM>(source.size()));
        try { result->imoex_candles = fetch_index_candles(); }
        catch (const std::exception& ex) { result->imoex_error = ex.what(); }
        for (const auto& item : source) {
            PostMessageW(target, WM_FETCH_PROGRESS, static_cast<WPARAM>(result->instruments.size() + 1), static_cast<LPARAM>(source.size()));
            try { result->instruments.push_back(fetch_instrument(item)); }
            catch (const std::exception& ex) {
                Instrument failed = item;
                failed.error = ex.what();
                result->instruments.push_back(std::move(failed));
            }
        }
        auto* raw = result.release();
        if (!PostMessageW(target, WM_DATA_READY, 0, reinterpret_cast<LPARAM>(raw))) delete raw;
    }).detach();
}

void Application::activate_page(int page) {
    app_active_page = std::clamp(page, 0, 5);
    if(signals_page.observation_page){ShowWindow(signals_page.observation_page,app_active_page==5?SW_SHOW:SW_HIDE);if(app_active_page==5)refresh_observation_page(signals_page.observation_page);}
    for (const auto control : charts_page.market_controls) ShowWindow(control, app_active_page == 0 ? SW_SHOW : SW_HIDE);
    for (const auto control : trades_page.controls) ShowWindow(control, app_active_page == 1 ? SW_SHOW : SW_HIDE);
    for (const auto control : portfolio_page.controls) ShowWindow(control, app_active_page == 2 ? SW_SHOW : SW_HIDE);
    for (const auto control : signals_page.controls) ShowWindow(control, app_active_page == 3 ? SW_SHOW : SW_HIDE);
    for (const auto control : backtest_page.controls) ShowWindow(control, app_active_page == 4 ? SW_SHOW : SW_HIDE);
    for (const auto control : app_global_controls) ShowWindow(control, SW_SHOW);
    InvalidateRect(app_window, nullptr, TRUE);
}

void Application::start_backup() {
    if (!IsWindowEnabled(app_backup_button)) return;
    // Flush the UI-owned state before the worker takes a consistent disk snapshot.
    // If loading failed, preserve the original files for recovery instead.
    if (!app_storage_blocked) { save_watchlist(); save_trades(); save_portfolio(); save_position_levels(); save_signal_settings(); }
    EnableWindow(app_backup_button, FALSE);
    const HWND target=app_window;
    std::thread([target] {
        auto result=std::make_unique<BackupResult>();
        try { result->path=storage::create_backup(storage::data_directory()); }
        catch(const std::exception& ex) { result->error=ex.what(); }
        auto* raw=result.release();
        if(!PostMessageW(target,WM_BACKUP_READY,0,reinterpret_cast<LPARAM>(raw)))delete raw;
    }).detach();
}

void Application::create_controls(HWND window) {
    app_font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    app_tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                            16, 8, 400, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TABS)), GetModuleHandleW(nullptr), nullptr);
    app_backup_button=CreateWindowExW(0,L"BUTTON",L"Резервная копия",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        1150,9,155,28,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BACKUP)),GetModuleHandleW(nullptr),nullptr);
    if(app_font)SendMessageW(app_backup_button,WM_SETFONT,reinterpret_cast<WPARAM>(app_font),TRUE);
    TCITEMW tab{}; tab.mask = TCIF_TEXT;
    tab.pszText = const_cast<wchar_t*>(L"Аналитика"); TabCtrl_InsertItem(app_tabs, 0, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Закрытые сделки"); TabCtrl_InsertItem(app_tabs, 1, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Портфель"); TabCtrl_InsertItem(app_tabs, 2, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Сигналы"); TabCtrl_InsertItem(app_tabs, 3, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Тест стратегии"); TabCtrl_InsertItem(app_tabs, 4, &tab);
    tab.pszText = const_cast<wchar_t*>(L"История сигналов"); TabCtrl_InsertItem(app_tabs, 5, &tab);
    TabCtrl_SetCurSel(app_tabs, 0);
    if (app_font) SendMessageW(app_tabs, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);

    create_charts_controls(window);
    create_trades_controls(window);
    create_portfolio_controls(window);
    create_signals_controls(window);
    create_backtest_controls(window);
    create_observation_controls(window);
    activate_page(0);
}

LRESULT CALLBACK Application::handle_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        app_window = window; create_controls(window); load_signal_settings(); synchronize_signal_controls();
        update_auto_refresh_timer(); load_watchlist(); load_observations(); load_trades(); load_portfolio(); load_position_levels(); load_market_cache(app_items, app_imoex_candles, app_imoex_error); fill_list(); fill_trades(); fill_signals(); fill_portfolio();
        for (const auto& item : app_items) {
            SendMessageW(backtest_page.ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(item.ticker).c_str()));
            SendMessageW(portfolio_page.ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(item.ticker).c_str()));
        }
        if (!app_items.empty()) { SendMessageW(backtest_page.ticker, CB_SETCURSEL, 0, 0); SendMessageW(portfolio_page.ticker, CB_SETCURSEL, 0, 0); }
        SetWindowTextW(portfolio_page.date, widen(today_date()).c_str());
        add_notification_icon(); refresh_data(); return 0;
    case WM_SIZE: {
        const int width = LOWORD(lparam), height = HIWORD(lparam);
        if(signals_page.observation_page)MoveWindow(signals_page.observation_page,16,100,width-32,height-116,TRUE);
        if (app_tabs) MoveWindow(app_tabs, 16, 8, width - 195, 34, TRUE);
        if(app_backup_button)MoveWindow(app_backup_button, width-171, 9, 155, 28, TRUE);
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
        case ID_REFRESH: refresh_data(); return 0;
        case ID_BACKUP: start_backup(); return 0;
        }
        break;
    case WM_NOTIFY:
        if(notify_charts(lparam))return 0;
        if(reinterpret_cast<NMHDR*>(lparam)->code==LVN_COLUMNCLICK) {
            const auto* click=reinterpret_cast<NMLISTVIEW*>(lparam);const int id=static_cast<int>(click->hdr.idFrom);
            if(id==ID_LIST||id==ID_TRADE_LIST||id==ID_PORT_POSITIONS||id==ID_PORT_TRANSACTIONS||id==ID_SIG_LIST){auto& sort=app_table_sorts[id];if(sort.column==click->iSubItem)sort.ascending=!sort.ascending;else{sort.column=click->iSubItem;sort.ascending=true;}apply_table_sort(click->hdr.hwndFrom,id);if(id==ID_PORT_POSITIONS)fill_position_level_editor();return 0;}
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_TABS && reinterpret_cast<NMHDR*>(lparam)->code == TCN_SELCHANGE) {
            activate_page(TabCtrl_GetCurSel(app_tabs)); return 0;
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_PORT_POSITIONS &&
            reinterpret_cast<NMHDR*>(lparam)->code == LVN_ITEMCHANGED) {
            if(app_filling)return 0;
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
        if (wparam == TIMER_AUTO_REFRESH && signals_page.settings.auto_refresh_enabled) { refresh_data(); return 0; }
        break;
    case WM_DATA_READY: {
        std::unique_ptr<MarketRefreshResult> data(reinterpret_cast<MarketRefreshResult*>(lparam));
        app_busy = false; EnableWindow(GetDlgItem(window, ID_REFRESH), TRUE);
        if (!data->imoex_candles.empty()) {
            app_imoex_candles = std::move(data->imoex_candles);
            app_imoex_stale = false;
            app_imoex_error.clear();
        } else {
            app_imoex_stale = true;
            app_imoex_error = std::move(data->imoex_error);
        }
        for (auto& fetched : data->instruments) {
            const auto it = std::find_if(app_items.begin(), app_items.end(), [&](const auto& existing) { return existing.ticker == fetched.ticker; });
            if (it != app_items.end()) {
                const auto current_target = it->target;
                fetched.dividends = std::move(it->dividends);
                fetched.dividend_error = std::move(it->dividend_error);
                fetched.dividends_checked_at = std::move(it->dividends_checked_at);
                fetched.dividend_note = std::move(it->dividend_note);
                fetched.dividends_stale = it->dividends_stale;
                *it = std::move(fetched);
                it->target = current_target;
            }
        }
        const auto loaded = std::count_if(app_items.begin(), app_items.end(), [](const auto& item) { return item.quote.last && !item.quote_stale; });
        std::wstring status = L"Котировки загружены: " + std::to_wstring(loaded) + L" из " + std::to_wstring(app_items.size()) + L". ";
        status += app_imoex_stale ? L"IMOEX не обновлён; сравнение использует старые данные. " : L"IMOEX обновлён. ";
        status += L"Подробности — в выбранной акции.";
        SetWindowTextW(charts_page.status, status.c_str());
        if (charts_page.dividend_window) { fill_dividend_list(); InvalidateRect(charts_page.dividend_window, nullptr, TRUE); }
        fill_list(); fill_signals(); fill_portfolio(); notify_new_signals(); return 0;
    }
    case WM_BACKUP_READY: {
        std::unique_ptr<BackupResult> result(reinterpret_cast<BackupResult*>(lparam));
        EnableWindow(app_backup_button,TRUE);
        if(!result->error.empty())MessageBoxW(window,widen("Резервная копия не создана: "+result->error).c_str(),L"MOEXWatch",MB_ICONERROR);
        else {
            MessageBoxW(window,(L"Копия создана и проверена:\n"+result->path.wstring()+
                L"\n\nДля восстановления закройте приложение и следуйте RESTORE.txt в этой папке.").c_str(),L"Резервная копия",MB_ICONINFORMATION);
            ShellExecuteW(window,L"open",result->path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        }
        return 0;
    }
    case WM_DIVIDENDS_READY:
        if (!charts_page.dividend_window || !PostMessageW(charts_page.dividend_window, message, wparam, lparam))
            delete reinterpret_cast<DividendFetchResult*>(lparam);
        return 0;
    case WM_BACKTEST_READY: {
        complete_backtest(lparam);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(window, &ps);
        if (app_active_page == 0) {
            RECT client{}; GetClientRect(window, &client);
            paint_charts(dc,client);
        }
        EndPaint(window, &ps); return 0;
    }
    case WM_DESTROY:
        KillTimer(window, TIMER_AUTO_REFRESH);
        save_watchlist(); save_trades(); save_portfolio(); save_position_levels(); save_signal_settings();
        if (signals_page.notification_icon_ready) Shell_NotifyIconW(NIM_DELETE, &signals_page.notification_icon);
        if (app_font) DeleteObject(app_font); PostQuitMessage(0); return 0;
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
    const auto result=app?app->handle_window_message(w,m,p,l):DefWindowProcW(w,m,p,l);
    if(m==WM_NCDESTROY)RemovePropW(w,L"MOEXWatch.Owner");
    return result;
}
LRESULT CALLBACK Application::dividends_window_proc(HWND w,UINT m,WPARAM p,LPARAM l) {
    auto* app=owner(w,m,l);
    const auto result=app?app->handle_dividends_message(w,m,p,l):DefWindowProcW(w,m,p,l);
    if(m==WM_NCDESTROY)RemovePropW(w,L"MOEXWatch.Owner");
    return result;
}
}

#endif
