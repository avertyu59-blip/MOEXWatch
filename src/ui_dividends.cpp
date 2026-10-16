#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
void Application::fill_dividend_list() {
    if (!charts_page.dividend_list || charts_page.dividend_instrument >= app_items.size()) return;
    ListView_DeleteAllItems(charts_page.dividend_list);
    const auto& item = app_items[charts_page.dividend_instrument];
    const auto as_of = today_date();
    const auto calendar = moex_calendar_2026();
    for (std::size_t index = 0; index < item.dividends.size(); ++index) {
        const auto& event = item.dividends[index];
        const auto calculated_yield = calculate_dividend_yield(event, today_date(), item.quote.last,
            item.quote_stale, item.candles, item.candles_stale);
        const std::wstring yield = calculated_yield.percent ? widen(percent(calculated_yield.percent) +
            (calculated_yield.basis == DividendYieldBasis::CurrentPrice ? " (к текущей)" : " (к закрытию)")) : L"н/д";
        LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(index); row.iItem = static_cast<int>(index);
        std::wstring date = event.record_date.empty() ? L"н/д (прогноз)" : widen(event.record_date);
        row.pszText = date.data();
        const int row_index = ListView_InsertItem(charts_page.dividend_list, &row);
        const auto set = [&](int column, std::wstring value) { ListView_SetItemText(charts_page.dividend_list, row_index, column, value.data()); };
        set(1, event.last_buy_date.empty() ? L"н/д" : widen(event.last_buy_date));
        set(2, widen(money(event.amount_per_share) + " " + event.currency)); set(3, std::move(yield));
        set(4, widen(dividend_status_text(event.status)));
        const std::string verification = event.verification == DividendVerification::Matched ? "Совпало с эмитентом" :
            event.verification == DividendVerification::Conflict ? "РАСХОЖДЕНИЕ с эмитентом" :
            event.verification == DividendVerification::Unavailable ? "Сайт эмитента недоступен" : "Не подтверждено эмитентом";
        set(5, widen(verification + (event.issuer_checked_at.empty() ? "" : " | " + event.issuer_checked_at)));
        set(6, widen(event.reported_last_buy.empty() ? "н/д" : event.reported_last_buy));
        set(7, widen(event.source_url)); set(8, widen(event.issuer_url)); set(9, widen(event.last_buy_basis));
        const auto entitlement = calculate_dividend_entitlement(item.ticker, event,
            portfolio_page.transactions, calendar, as_of);
        std::string expected = "н/д: " + entitlement.reason;
        if (item.dividends_stale) expected = "н/д: актуальность события не подтверждена";
        else if (entitlement.gross_amount) expected = money(*entitlement.quantity) + " акц.; " +
            money(*entitlement.gross_amount) + " RUB; " + entitlement.reason;
        set(10, widen(expected));
    }
    if (charts_page.dividend_info) {
        std::string info = "Обновлено: " + (item.dividends_checked_at.empty() ? "ещё не загружено" : item.dividends_checked_at) +
            (item.dividends_stale ? " | сохранённые данные, актуальность не подтверждена" : "") +
            (item.dividends.empty() ? " | событий в полученных таблицах нет" : "") +
            "\r\nSmart-Lab и таблица ЛУКОЙЛ. Двойной щелчок — открыть источник (столбец «Эмитент» — официальный сайт).\r\n¹ Расчёт T+1 по опубликованному расписанию MOEX 2026; вне покрытия — н/д. ² До налогов. Прогноз и рекомендация не гарантируют выплату.";
        if (!item.dividend_note.empty()) info += "\r\n" + item.dividend_note;
        if (!item.dividend_error.empty()) info += "\r\nОбновление не удалось: " + item.dividend_error;
        SetWindowTextW(charts_page.dividend_info, widen(info).c_str());
    }
    apply_table_sort(charts_page.dividend_list,ID_DIVIDENDS_LIST);
}

void Application::refresh_dividend_events() {
    if (!charts_page.dividend_window || charts_page.dividend_instrument >= app_items.size()) return;
    const std::string ticker = app_items[charts_page.dividend_instrument].ticker;
    if (charts_page.dividend_info) SetWindowTextW(charts_page.dividend_info, L"Загружаю открытый календарь дивидендов…");
    const HWND target = app_window;
    const auto request = ++charts_page.dividend_request;
    EnableWindow(charts_page.dividend_refresh, FALSE);
    std::thread([target, ticker, request]() {
        auto result = std::make_unique<DividendFetchResult>();
        result->ticker = ticker;
        result->checked_at = local_timestamp();
        try { fetch_dividend_events(*result); }
        catch (const std::exception& ex) { result->error = ex.what(); }
        auto* raw = result.release();
        if (!PostMessageW(target, WM_DIVIDENDS_READY, request, reinterpret_cast<LPARAM>(raw))) delete raw;
    }).detach();
}

void Application::open_dividends_window() {
    const int selected = selected_instrument();
    if (selected < 0 || static_cast<std::size_t>(selected) >= app_items.size()) {
        MessageBoxW(app_window, L"Сначала выберите акцию в списке.", L"Дивиденды", MB_ICONINFORMATION);
        return;
    }
    charts_page.dividend_instrument = static_cast<std::size_t>(selected);
    try { load_dividend_cache(app_items[charts_page.dividend_instrument]); }
    catch (const std::exception& ex) { app_items[charts_page.dividend_instrument].dividend_note = ex.what(); }
    if (charts_page.dividend_window) {
        SetWindowLongPtrW(charts_page.dividend_window, GWLP_USERDATA, static_cast<LONG_PTR>(charts_page.dividend_instrument));
        SetWindowTextW(charts_page.dividend_window, (L"Дивидендная аналитика — " + widen(app_items[charts_page.dividend_instrument].ticker)).c_str());
        fill_dividend_list(); InvalidateRect(charts_page.dividend_window, nullptr, TRUE); ShowWindow(charts_page.dividend_window, SW_RESTORE);
        SetForegroundWindow(charts_page.dividend_window); refresh_dividend_events(); return;
    }
    charts_page.dividend_window = CreateWindowExW(0, L"MOEXWatchDividendWindow", L"Дивидендная аналитика",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 840, app_window, nullptr,
        GetModuleHandleW(nullptr), this);
    if (!charts_page.dividend_window) {
        MessageBoxW(app_window, L"Не удалось открыть окно дивидендной аналитики.", L"Ошибка", MB_ICONERROR);
        return;
    }
    SetWindowTextW(charts_page.dividend_window, (L"Дивидендная аналитика — " + widen(app_items[charts_page.dividend_instrument].ticker)).c_str());
    ShowWindow(charts_page.dividend_window, SW_SHOW); UpdateWindow(charts_page.dividend_window);
}

LRESULT CALLBACK Application::handle_dividends_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        charts_page.dividend_window = window;
        charts_page.dividend_info = CreateWindowExW(0, L"STATIC", L"Загружаю открытый календарь дивидендов…", WS_CHILD | WS_VISIBLE | SS_LEFT,
            12, 10, 1000, 92, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        charts_page.dividend_refresh = CreateWindowExW(0, L"BUTTON", L"Обновить события", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1015, 8, 160, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIVIDENDS_REFRESH)), GetModuleHandleW(nullptr), nullptr);
        charts_page.dividend_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            12, 110, 1140, 165, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIVIDENDS_LIST)), GetModuleHandleW(nullptr), nullptr);
        ListView_SetExtendedListViewStyle(charts_page.dividend_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        const wchar_t* headers[] = {L"Дата отсечки", L"Последний день покупки¹", L"На акцию", L"Доходность²", L"Статус", L"Сверка с эмитентом", L"Купить до (источник)", L"Источник", L"Эмитент", L"Основание расчёта¹", L"Ожидается по журналу²"};
        const int widths[] = {115, 180, 125, 160, 230, 280, 170, 340, 340, 460, 540};
        for (int i = 0; i < 11; ++i) {
            LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(headers[i]); column.cx = widths[i];
            ListView_InsertColumn(charts_page.dividend_list, i, &column);
        }
        if (app_font) {
            SendMessageW(charts_page.dividend_info, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
            SendMessageW(charts_page.dividend_list, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
            SendMessageW(charts_page.dividend_refresh, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
        }
        charts_page.dividend_chart_tabs = create_chart_tabs(window, ID_DIV_CHART_TABS, 12, 289, 800);
        charts_page.dividend_chart_range = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            870, 289, 280, 180, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIV_CHART_RANGE)), GetModuleHandleW(nullptr), nullptr);
        if (app_font) SendMessageW(charts_page.dividend_chart_range, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
        const wchar_t* ranges[] = {L"1 месяц (~22 сессии)", L"3 месяца (~66 сессий)", L"1 год (~252 сессии)", L"Вся загруженная история"};
        for (const auto* range : ranges) SendMessageW(charts_page.dividend_chart_range, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(range));
        charts_page.dividend_chart_panel = ChartPanel::Price; charts_page.dividend_chart_sessions = 66;
        SendMessageW(charts_page.dividend_chart_range, CB_SETCURSEL, 1, 0);
        fill_dividend_list(); refresh_dividend_events();
        return 0;
    }
    case WM_SIZE: {
        const int width = LOWORD(lparam);
        if (charts_page.dividend_info) MoveWindow(charts_page.dividend_info, 12, 10, width - 185, 92, TRUE);
        if (charts_page.dividend_refresh) MoveWindow(charts_page.dividend_refresh, width - 172, 8, 160, 28, TRUE);
        if (charts_page.dividend_list) MoveWindow(charts_page.dividend_list, 12, 110, width - 24, 165, TRUE);
        if (charts_page.dividend_chart_tabs) MoveWindow(charts_page.dividend_chart_tabs, 12, 289, width - 315, 30, TRUE);
        if (charts_page.dividend_chart_range) MoveWindow(charts_page.dividend_chart_range, width - 292, 289, 280, 180, TRUE);
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_DIVIDENDS_REFRESH) { refresh_dividend_events(); return 0; }
        if (LOWORD(wparam) == ID_DIV_CHART_RANGE && HIWORD(wparam) == CBN_SELCHANGE) {
            const int ranges[] = {22, 66, 252, 0};
            const auto index = SendMessageW(charts_page.dividend_chart_range, CB_GETCURSEL, 0, 0);
            if (index >= 0 && index < 4) charts_page.dividend_chart_sessions = ranges[index];
            InvalidateRect(window, nullptr, TRUE); return 0;
        }
        break;
    case WM_NOTIFY: {
        const auto* notice = reinterpret_cast<const NMHDR*>(lparam);
        if(notice->idFrom==ID_DIVIDENDS_LIST && notice->code==LVN_COLUMNCLICK) {
            auto& sort=app_table_sorts[ID_DIVIDENDS_LIST];const int column=reinterpret_cast<NMLISTVIEW*>(lparam)->iSubItem;
            if(sort.column==column)sort.ascending=!sort.ascending;else{sort.column=column;sort.ascending=true;}
            apply_table_sort(charts_page.dividend_list,ID_DIVIDENDS_LIST);return 0;
        }
        if (notice->idFrom == ID_DIVIDENDS_LIST && notice->code == NM_DBLCLK) {
            const auto* click = reinterpret_cast<const NMITEMACTIVATE*>(lparam);
            if (charts_page.dividend_instrument < app_items.size() && click->iItem >= 0 &&
                static_cast<std::size_t>(click->iItem) < app_items[charts_page.dividend_instrument].dividends.size()) {
                const auto event_index=row_data(charts_page.dividend_list,click->iItem);
                if(!event_index || *event_index>=app_items[charts_page.dividend_instrument].dividends.size())return 0;
                const auto& event = app_items[charts_page.dividend_instrument].dividends[*event_index];
                const auto& url = click->iSubItem == 8 ? event.issuer_url : event.source_url;
                if (url.starts_with("https://smart-lab.ru/") || url.starts_with("https://lukoil.ru/"))
                    ShellExecuteW(window, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            return 0;
        }
        if (notice->idFrom == ID_DIV_CHART_TABS && notice->code == TCN_SELCHANGE) {
            charts_page.dividend_chart_panel = static_cast<ChartPanel>(std::clamp(TabCtrl_GetCurSel(charts_page.dividend_chart_tabs), 0, 3));
            InvalidateRect(window, nullptr, TRUE); return 0;
        }
        break;
    }
    case WM_DIVIDENDS_READY: {
        std::unique_ptr<DividendFetchResult> result(reinterpret_cast<DividendFetchResult*>(lparam));
        if (wparam != charts_page.dividend_request) return 0;
        EnableWindow(charts_page.dividend_refresh, TRUE);
        const auto it = std::find_if(app_items.begin(), app_items.end(), [&](const auto& item) { return item.ticker == result->ticker; });
        const bool is_current = it != app_items.end() && charts_page.dividend_instrument < app_items.size() &&
            app_items[charts_page.dividend_instrument].ticker == result->ticker;
        if (it != app_items.end()) {
            it->dividend_error = std::move(result->error);
            if (it->dividend_error.empty()) {
                it->dividends = std::move(result->events);
                it->dividends_stale = false;
                it->dividends_checked_at = std::move(result->checked_at);
                it->dividend_note = std::move(result->note);
                try { save_dividend_cache(*it); }
                catch (const std::exception& ex) { it->dividend_note += " | " + std::string(ex.what()); }
            } else it->dividends_stale = true;
            if (is_current) {
                charts_page.dividend_instrument = static_cast<std::size_t>(it - app_items.begin());
                fill_dividend_list();
                InvalidateRect(window, nullptr, TRUE);
            }
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(window, &ps);
        RECT client{}; GetClientRect(window, &client);
        RECT chart{client.left + 8, 330, client.right - 8, client.bottom - 8};
        const auto old_font = app_font ? SelectObject(dc, app_font) : nullptr;
        draw_chart(dc, chart, charts_page.dividend_instrument, charts_page.dividend_chart_panel, charts_page.dividend_chart_sessions);
        if (app_font) SelectObject(dc, old_font);
        EndPaint(window, &ps); return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize.x = 1000; limits->ptMinTrackSize.y = 760; return 0;
    }
    case WM_NCDESTROY:
        ++charts_page.dividend_request;
        charts_page.dividend_chart_tabs = nullptr; charts_page.dividend_chart_range = nullptr;
        charts_page.dividend_window = nullptr; charts_page.dividend_list = nullptr; charts_page.dividend_info = nullptr; charts_page.dividend_refresh = nullptr;
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
#endif
