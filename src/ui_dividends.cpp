#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
void Application::fill_dividend_list() {
    if (!dividends_page.list || dividend_instrument_index() >= state.market.instruments.size()) return;
    const ScopedViewUpdate update(dividends_page.updating);
    ListView_DeleteAllItems(dividends_page.list);
    const auto& item = state.market.instruments[dividend_instrument_index()];
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
        const int row_index = ListView_InsertItem(dividends_page.list, &row);
        const auto set = [&](int column, std::wstring value) { ListView_SetItemText(dividends_page.list, row_index, column, value.data()); };
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
    if (dividends_page.info) {
        std::string info = "Обновлено: " + (item.dividends_checked_at.empty() ? "ещё не загружено" : item.dividends_checked_at) +
            (item.dividends_stale ? " | сохранённые данные, актуальность не подтверждена" : "") +
            (item.dividends.empty() ? " | событий в полученных таблицах нет" : "") +
            "\r\nSmart-Lab и таблица ЛУКОЙЛ. Двойной щелчок — открыть источник (столбец «Эмитент» — официальный сайт).\r\n¹ Расчёт T+1 по опубликованному расписанию MOEX 2026; вне покрытия — н/д. ² До налогов. Прогноз и рекомендация не гарантируют выплату.";
        if (!item.dividend_note.empty()) info += "\r\n" + item.dividend_note;
        if (!item.dividend_error.empty()) info += "\r\nОбновление не удалось: " + item.dividend_error;
        SetWindowTextW(dividends_page.info, widen(info).c_str());
    }
    apply_table_sort(dividends_page.list,ID_DIVIDENDS_LIST);
}

void Application::refresh_dividend_events() {
    if (!dividends_page.window || dividend_instrument_index() >= state.market.instruments.size()) return;
    const std::string ticker = state.market.instruments[dividend_instrument_index()].ticker;
    if (dividends_page.info) SetWindowTextW(dividends_page.info, L"Загружаю открытый календарь дивидендов…");
    const HWND target = shell.window;
    const auto request = ++dividends_page.request;
    EnableWindow(dividends_page.refresh, FALSE);
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
    if (selected < 0 || static_cast<std::size_t>(selected) >= state.market.instruments.size()) {
        MessageBoxW(shell.window, L"Сначала выберите акцию в списке.", L"Дивиденды", MB_ICONINFORMATION);
        return;
    }
    dividends_page.ticker = state.market.instruments[static_cast<std::size_t>(selected)].ticker;
    try { load_dividend_cache(state.market.instruments[dividend_instrument_index()]); }
    catch (const std::exception& ex) { state.market.instruments[dividend_instrument_index()].dividend_note = ex.what(); }
    if (dividends_page.window) {
        SetWindowTextW(dividends_page.window, (L"Дивидендная аналитика — " + widen(state.market.instruments[dividend_instrument_index()].ticker)).c_str());
        fill_dividend_list(); InvalidateRect(dividends_page.window, nullptr, TRUE); ShowWindow(dividends_page.window, SW_RESTORE);
        SetForegroundWindow(dividends_page.window); refresh_dividend_events(); return;
    }
    dividends_page.window = CreateWindowExW(0, L"MOEXWatchDividendWindow", L"Дивидендная аналитика",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 840, shell.window, nullptr,
        GetModuleHandleW(nullptr), this);
    if (!dividends_page.window) {
        MessageBoxW(shell.window, L"Не удалось открыть окно дивидендной аналитики.", L"Ошибка", MB_ICONERROR);
        return;
    }
    SetWindowTextW(dividends_page.window, (L"Дивидендная аналитика — " + widen(state.market.instruments[dividend_instrument_index()].ticker)).c_str());
    ShowWindow(dividends_page.window, SW_SHOW); UpdateWindow(dividends_page.window);
}

LRESULT CALLBACK Application::handle_dividends_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        dividends_page.window = window;
        dividends_page.info = CreateWindowExW(0, L"STATIC", L"Загружаю открытый календарь дивидендов…", WS_CHILD | WS_VISIBLE | SS_LEFT,
            12, 10, 1000, 92, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        dividends_page.refresh = CreateWindowExW(0, L"BUTTON", L"Обновить события", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1015, 8, 160, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIVIDENDS_REFRESH)), GetModuleHandleW(nullptr), nullptr);
        dividends_page.list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            12, 110, 1140, 165, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIVIDENDS_LIST)), GetModuleHandleW(nullptr), nullptr);
        ListView_SetExtendedListViewStyle(dividends_page.list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        const wchar_t* headers[] = {L"Дата отсечки", L"Последний день покупки¹", L"На акцию", L"Доходность²", L"Статус", L"Сверка с эмитентом", L"Купить до (источник)", L"Источник", L"Эмитент", L"Основание расчёта¹", L"Ожидается по журналу²"};
        const int widths[] = {115, 180, 125, 160, 230, 280, 170, 340, 340, 460, 540};
        for (int i = 0; i < 11; ++i) {
            LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(headers[i]); column.cx = widths[i];
            ListView_InsertColumn(dividends_page.list, i, &column);
        }
        if (shell.font) {
            SendMessageW(dividends_page.info, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
            SendMessageW(dividends_page.list, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
            SendMessageW(dividends_page.refresh, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
        }
        dividends_page.chart_tabs = create_chart_tabs(window, ID_DIV_CHART_TABS, 12, 289, 800);
        dividends_page.chart_range = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            870, 289, 280, 180, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIV_CHART_RANGE)), GetModuleHandleW(nullptr), nullptr);
        if (shell.font) SendMessageW(dividends_page.chart_range, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
        const wchar_t* ranges[] = {L"1 месяц (~22 сессии)", L"3 месяца (~66 сессий)", L"1 год (~252 сессии)", L"Вся загруженная история"};
        for (const auto* range : ranges) SendMessageW(dividends_page.chart_range, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(range));
        dividends_page.chart_panel = ChartPanel::Price; dividends_page.chart_sessions = 66;
        SendMessageW(dividends_page.chart_range, CB_SETCURSEL, 1, 0);
        fill_dividend_list(); refresh_dividend_events();
        return 0;
    }
    case WM_SIZE: {
        const int width = LOWORD(lparam);
        if (dividends_page.info) MoveWindow(dividends_page.info, 12, 10, width - 185, 92, TRUE);
        if (dividends_page.refresh) MoveWindow(dividends_page.refresh, width - 172, 8, 160, 28, TRUE);
        if (dividends_page.list) MoveWindow(dividends_page.list, 12, 110, width - 24, 165, TRUE);
        if (dividends_page.chart_tabs) MoveWindow(dividends_page.chart_tabs, 12, 289, width - 315, 30, TRUE);
        if (dividends_page.chart_range) MoveWindow(dividends_page.chart_range, width - 292, 289, 280, 180, TRUE);
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_DIVIDENDS_REFRESH) { refresh_dividend_events(); return 0; }
        if (LOWORD(wparam) == ID_DIV_CHART_RANGE && HIWORD(wparam) == CBN_SELCHANGE) {
            const int ranges[] = {22, 66, 252, 0};
            const auto index = SendMessageW(dividends_page.chart_range, CB_GETCURSEL, 0, 0);
            if (index >= 0 && index < 4) dividends_page.chart_sessions = ranges[index];
            InvalidateRect(window, nullptr, TRUE); return 0;
        }
        break;
    case WM_NOTIFY: {
        const auto* notice = reinterpret_cast<const NMHDR*>(lparam);
        if(notice->idFrom==ID_DIVIDENDS_LIST && notice->code==LVN_COLUMNCLICK) {
            auto& sort=dividends_page.sort;const int column=reinterpret_cast<NMLISTVIEW*>(lparam)->iSubItem;
            if(sort.column==column)sort.ascending=!sort.ascending;else{sort.column=column;sort.ascending=true;}
            apply_table_sort(dividends_page.list,ID_DIVIDENDS_LIST);return 0;
        }
        if (notice->idFrom == ID_DIVIDENDS_LIST && notice->code == NM_DBLCLK) {
            const auto* click = reinterpret_cast<const NMITEMACTIVATE*>(lparam);
            if (dividend_instrument_index() < state.market.instruments.size() && click->iItem >= 0 &&
                static_cast<std::size_t>(click->iItem) < state.market.instruments[dividend_instrument_index()].dividends.size()) {
                const auto event_index=row_data(dividends_page.list,click->iItem);
                if(!event_index || *event_index>=state.market.instruments[dividend_instrument_index()].dividends.size())return 0;
                const auto& event = state.market.instruments[dividend_instrument_index()].dividends[*event_index];
                const auto& url = click->iSubItem == 8 ? event.issuer_url : event.source_url;
                if (url.starts_with("https://smart-lab.ru/") || url.starts_with("https://lukoil.ru/"))
                    ShellExecuteW(window, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            return 0;
        }
        if (notice->idFrom == ID_DIV_CHART_TABS && notice->code == TCN_SELCHANGE) {
            dividends_page.chart_panel = static_cast<ChartPanel>(std::clamp(TabCtrl_GetCurSel(dividends_page.chart_tabs), 0, 3));
            InvalidateRect(window, nullptr, TRUE); return 0;
        }
        break;
    }
    case WM_DIVIDENDS_READY: {
        std::unique_ptr<DividendFetchResult> result(reinterpret_cast<DividendFetchResult*>(lparam));
        if (wparam != dividends_page.request) return 0;
        EnableWindow(dividends_page.refresh, TRUE);
        const auto it = std::find_if(state.market.instruments.begin(), state.market.instruments.end(), [&](const auto& item) { return item.ticker == result->ticker; });
        const bool is_current = it != state.market.instruments.end() && dividend_instrument_index() < state.market.instruments.size() &&
            state.market.instruments[dividend_instrument_index()].ticker == result->ticker;
        if (it != state.market.instruments.end()) {
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
        const auto old_font = shell.font ? SelectObject(dc, shell.font) : nullptr;
        draw_chart(dc, chart, dividend_instrument_index(), dividends_page.chart_panel, dividends_page.chart_sessions);
        if (shell.font) SelectObject(dc, old_font);
        EndPaint(window, &ps); return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize.x = 1000; limits->ptMinTrackSize.y = 760; return 0;
    }
    case WM_NCDESTROY:
        ++dividends_page.request;
        dividends_page.ticker.clear();
        dividends_page.chart_tabs = nullptr; dividends_page.chart_range = nullptr;
        dividends_page.window = nullptr; dividends_page.list = nullptr; dividends_page.info = nullptr; dividends_page.refresh = nullptr;
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
#endif
