#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
int Application::selected_instrument() {
    const auto selected=selected_row_data(charts_page.list);
    return selected && *selected<app_items.size()?static_cast<int>(*selected):-1;
}

void Application::save_watchlist() {
    if (app_storage_blocked) return;
    try { storage::save_watchlist(storage::data_directory(), app_items); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void Application::load_watchlist() {
    try { auto loaded = app_items; storage::load_watchlist(storage::data_directory(), loaded); app_items = std::move(loaded); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

std::string Application::status_for(const Instrument& item) {
    if (!item.quote.last) return !item.candles.empty() && item.candles_stale ? "График из кэша; нет котировки" : (item.error.empty() ? "Нет котировки" : "Ошибка данных");
    if (item.quote_stale) return "Котировка устарела";
    if (item.target && *item.quote.last <= *item.target) return "Целевая цена достигнута";
    if (item.candles.empty()) return "Нет истории";
    if (item.candles_stale) return "График устарел";
    const auto analysis = analyze(item.candles);
    if (analysis.rsi14 && *analysis.rsi14 < 30.0) return "Перепроданность";
    if (analysis.rsi14 && *analysis.rsi14 > 70.0) return "Перекупленность";
    return "Данные получены";
}

void Application::fill_list() {
    app_filling = true;
    ListView_DeleteAllItems(charts_page.list);
    charts_page.visible.clear();
    for (std::size_t i = 0; i < app_items.size(); ++i) {
        const auto& item = app_items[i];
        const auto analysis = analyze(item.candles);
        const bool stale = item.quote_stale ||
            (charts_page.filter != WatchFilter::NearTarget && item.candles_stale);
        if (!matches_filter(analysis, item.quote.last, item.target, stale, charts_page.filter)) continue;
        const int row_index = static_cast<int>(charts_page.visible.size());
        charts_page.visible.push_back(i);
        LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(i); row.iItem = row_index;
        auto ticker = widen(item.ticker); row.pszText = ticker.data();
        ListView_InsertItem(charts_page.list, &row);
        const std::string values[] = {optional_number(item.quote.last), percent(item.quote.change_percent),
            optional_number(item.target), optional_number(analysis.rsi14), status_for(item)};
        for (int col = 1; col <= 5; ++col) {
            auto wide = widen(values[col - 1]); ListView_SetItemText(charts_page.list, row_index, col, wide.data());
        }
    }
    if (!charts_page.visible.empty()) {
        const auto selected = std::find(charts_page.visible.begin(), charts_page.visible.end(), charts_page.selected);
        const auto row = selected == charts_page.visible.end() ? 0 : static_cast<int>(selected - charts_page.visible.begin());
        charts_page.selected = charts_page.visible[static_cast<std::size_t>(row)];
        ListView_SetItemState(charts_page.list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    apply_table_sort(charts_page.list,ID_LIST);
    app_filling = false;
    update_analysis();
}

void Application::update_analysis() {
    if (!charts_page.analysis) return;
    if (app_items.empty() || charts_page.visible.empty()) {
        SetWindowTextW(charts_page.analysis, L"Нет акций, соответствующих фильтру. Выберите «Все акции». ");
        InvalidateRect(app_window, nullptr, TRUE); return;
    }
    charts_page.selected = std::min(charts_page.selected, app_items.size() - 1);
    const auto& item = app_items[charts_page.selected];
    const auto a = analyze(item.candles);
    std::ostringstream text;
    text << item.ticker << "   |   Последняя цена: " << optional_number(item.quote.last) << " RUB\r\n"
         << "Изменение к предыдущей сделке: " << percent(item.quote.change_percent) << "\r\n"
         << "Целевая цена: " << optional_number(item.target);
    if (item.target && item.quote.last && *item.target > 0) text << "  (" << std::fixed << std::setprecision(1) << ((*item.quote.last / *item.target - 1.0) * 100.0) << "% от цели)";
    text << "\r\nSMA20: " << optional_number(a.sma20) << "  SMA50: " << optional_number(a.sma50)
         << "  SMA200: " << optional_number(a.sma200) << "\r\n"
         << "RSI14 (Уайлдер): " << optional_number(a.rsi14) << "  Объём / средний20: " << optional_number(a.volume_ratio20) << "x\r\n"
         << "ATR14: " << optional_number(a.atr14) << " ₽ (" << percent(a.atr_percent) << ")\r\n"
         << "От максимума истории: " << percent(a.drawdown_percent) << "\r\n"
         << "Тренд по дневному закрытию: " << (a.sma200 ? (item.candles.back().close > *a.sma200 ? "выше SMA200" : "ниже/равно SMA200") : "для SMA200 нужны 200 сессий") << "\r\n"
         << "Индикаторы: " << a.technical_note << "\r\n"
         << "Котировка: " << (item.quote_stale ? "устарела или недоступна" : (item.quote.updated_at.empty() ? "время обновления не указано" : item.quote.updated_at))
         << "  |  график: " << (item.candles_stale ? "сохранённый / не обновлён" : "обновлён")
         << (item.candles_checked_at.empty() ? "" : " " + item.candles_checked_at)
         << "  |  свечей: " << item.candles.size();
    if (!item.error.empty()) text << "\r\nСтатус источника: " << item.error;
    if (!app_imoex_error.empty()) text << "\r\nИстория IMOEX: " << app_imoex_error;
    text << "\r\nТехнические индикаторы описывают прошлые цены и не являются сигналом к покупке.";
    SetWindowTextW(charts_page.analysis, widen(text.str()).c_str());
    InvalidateRect(app_window, nullptr, TRUE);
}

void Application::add_ticker() {
    std::string ticker = narrow(read_edit(charts_page.ticker_edit));
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (!valid_ticker(ticker)) { MessageBoxW(app_window, L"Введите тикер латиницей, например SBER.", L"Некорректный тикер", MB_ICONWARNING); return; }
    if (app_items.size() >= 30) { MessageBoxW(app_window, L"В списке может быть не более 30 тикеров.", L"Лимит списка", MB_ICONINFORMATION); return; }
    if (std::any_of(app_items.begin(), app_items.end(), [&](const auto& x) { return x.ticker == ticker; })) {
        MessageBoxW(app_window, L"Этот тикер уже есть в списке.", L"Список наблюдения", MB_ICONINFORMATION); return;
    }
    Instrument item; item.ticker = ticker; item.quote.ticker = ticker;
    signals_page.observer.forget_ticker(ticker);
    app_items.push_back(std::move(item)); charts_page.selected = app_items.size() - 1;
    SendMessageW(backtest_page.ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(ticker).c_str()));
    SendMessageW(portfolio_page.ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(ticker).c_str()));
    SendMessageW(portfolio_page.ticker, CB_SETCURSEL, static_cast<WPARAM>(app_items.size() - 1), 0);
    SendMessageW(backtest_page.ticker, CB_SETCURSEL, static_cast<WPARAM>(app_items.size() - 1), 0);
    SetWindowTextW(charts_page.ticker_edit, L""); save_watchlist(); fill_list(); fill_signals(); signals_page.baseline_ready = false; refresh_data();
}

void Application::set_target() {
    const int selected = selected_instrument();
    if (selected < 0 || static_cast<std::size_t>(selected) >= app_items.size()) return;
    const auto text = narrow(read_edit(charts_page.target_edit));
    if (text.empty()) app_items[static_cast<std::size_t>(selected)].target.reset();
    else {
        try {
            std::string normalized = text;
            std::replace(normalized.begin(), normalized.end(), ',', '.');
            std::size_t used = 0; const double value = std::stod(normalized, &used);
            if (used != normalized.size() || !std::isfinite(value) || value <= 0.0) throw std::invalid_argument("target");
            app_items[static_cast<std::size_t>(selected)].target = value;
        } catch (...) { MessageBoxW(app_window, L"Введите положительную цену. Для очистки оставьте поле пустым.", L"Некорректная цель", MB_ICONWARNING); return; }
    }
    save_watchlist(); fill_list(); fill_signals(); signals_page.baseline_ready = false;
}

void Application::remove_selected() {
    const int selected = selected_instrument();
    if (selected < 0 || static_cast<std::size_t>(selected) >= app_items.size()) return;
    const auto portfolio = calculate_portfolio(portfolio_page.transactions, {});
    if (portfolio && std::any_of(portfolio->positions.begin(), portfolio->positions.end(), [&](const auto& position) {
            return position.ticker == app_items[static_cast<std::size_t>(selected)].ticker && position.quantity > 0.0;
        })) {
        MessageBoxW(app_window, L"Тикер нельзя убрать из наблюдения, пока по нему есть открытая позиция.", L"Открытая позиция", MB_ICONINFORMATION);
        return;
    }
    if (charts_page.dividend_window) {
        if (static_cast<std::size_t>(selected) == charts_page.dividend_instrument) DestroyWindow(charts_page.dividend_window);
        else if (static_cast<std::size_t>(selected) < charts_page.dividend_instrument) --charts_page.dividend_instrument;
    }
    signals_page.observer.forget_ticker(app_items[static_cast<std::size_t>(selected)].ticker);
    app_items.erase(app_items.begin() + selected);
    SendMessageW(portfolio_page.ticker, CB_DELETESTRING, static_cast<WPARAM>(selected), 0);
    if (!app_items.empty()) SendMessageW(portfolio_page.ticker, CB_SETCURSEL, static_cast<WPARAM>(std::min(static_cast<std::size_t>(selected), app_items.size() - 1)), 0);
    SendMessageW(backtest_page.ticker, CB_DELETESTRING, static_cast<WPARAM>(selected), 0);
    if (!app_items.empty()) SendMessageW(backtest_page.ticker, CB_SETCURSEL, static_cast<WPARAM>(std::min(static_cast<std::size_t>(selected), app_items.size() - 1)), 0);
    if (app_items.empty()) charts_page.selected = 0; else charts_page.selected = std::min(static_cast<std::size_t>(selected), app_items.size() - 1);
    save_watchlist(); fill_list(); fill_signals(); signals_page.baseline_ready = false; InvalidateRect(app_window, nullptr, TRUE);
}

void Application::create_charts_controls(HWND window) {
    add_control(charts_page.market_controls,window, L"STATIC", L"Тикер", 0, 16, 61, 48, 24, 0);
    add_control(charts_page.market_controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 66, 57, 105, 28, ID_TICKER);
    charts_page.ticker_edit = GetDlgItem(window, ID_TICKER);
    add_control(charts_page.market_controls,window, L"BUTTON", L"Добавить", BS_PUSHBUTTON, 180, 57, 95, 28, ID_ADD);
    add_control(charts_page.market_controls,window, L"STATIC", L"Целевая цена, ₽", 0, 290, 61, 125, 24, 0);
    add_control(charts_page.market_controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 420, 57, 105, 28, ID_TARGET);
    charts_page.target_edit = GetDlgItem(window, ID_TARGET);
    add_control(charts_page.market_controls,window, L"BUTTON", L"Сохранить цель", BS_PUSHBUTTON, 535, 57, 125, 28, ID_TARGET_SET);
    add_control(charts_page.market_controls,window, L"BUTTON", L"Удалить акцию", BS_PUSHBUTTON, 670, 57, 115, 28, ID_REMOVE);
    add_control(charts_page.market_controls,window, L"BUTTON", L"Обновить", BS_PUSHBUTTON, 795, 57, 105, 28, ID_REFRESH);
    add_control(charts_page.market_controls,window, L"BUTTON", L"Дивиденды", BS_PUSHBUTTON, 914, 57, 125, 28, ID_DIVIDENDS_OPEN);
    add_control(charts_page.market_controls,window, L"STATIC", L"Данные MOEX ISS могут отображаться с задержкой до 15 минут. Это приложение не отправляет сделки.", 0, 16, 94, 950, 22, 0);
    charts_page.status = CreateWindowExW(0, L"STATIC", L"Готово", WS_CHILD | WS_VISIBLE, 16, 119, 940, 24, window, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (app_font) SendMessageW(charts_page.status, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
    charts_page.market_controls.push_back(charts_page.status);
    add_control(charts_page.market_controls,window, L"STATIC", L"Фильтр", 0, 16, 153, 58, 24, 0);
    add_control(charts_page.market_controls,window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 78, 149, 265, 220, ID_FILTER);
    const wchar_t* filters[] = {L"Все акции", L"Перепроданность: RSI < 30", L"Перекупленность: RSI > 70", L"Цена выше SMA200", L"До цели не более 5%"};
    for (const auto* name : filters) SendMessageW(GetDlgItem(window, ID_FILTER), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    SendMessageW(GetDlgItem(window, ID_FILTER), CB_SETCURSEL, 0, 0);
    add_control(charts_page.market_controls,window, L"STATIC", L"График", 0, 375, 153, 60, 24, 0);
    add_control(charts_page.market_controls,window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 440, 149, 260, 180, ID_RANGE);
    const wchar_t* ranges[] = {L"1 месяц (~22 сессии)", L"3 месяца (~66 сессий)", L"1 год (~252 сессии)", L"Вся загруженная история"};
    for (const auto* name : ranges) SendMessageW(GetDlgItem(window, ID_RANGE), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    SendMessageW(GetDlgItem(window, ID_RANGE), CB_SETCURSEL, 1, 0);
    charts_page.list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                             16, 184, 550, 630, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LIST)), GetModuleHandleW(nullptr), nullptr);
    charts_page.market_controls.push_back(charts_page.list);
    ListView_SetExtendedListViewStyle(charts_page.list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* headers[] = {L"Тикер", L"Цена", L"Изм.%", L"Цель", L"RSI14", L"Статус"};
    const int widths[] = {65, 76, 68, 70, 60, 130};
    for (int i = 0; i < 6; ++i) { LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = const_cast<wchar_t*>(headers[i]); col.cx = widths[i]; ListView_InsertColumn(charts_page.list, i, &col); }
    if (app_font) SendMessageW(charts_page.list, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
    charts_page.analysis = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 610, 184, 690, 245, window, nullptr, GetModuleHandleW(nullptr), nullptr);
    charts_page.market_controls.push_back(charts_page.analysis);
    if (app_font) SendMessageW(charts_page.analysis, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
    charts_page.chart_tabs = create_chart_tabs(window, ID_CHART_TABS, 610, 435, 690);
    charts_page.market_controls.push_back(charts_page.chart_tabs);

}

void Application::resize_charts(int width, int height) {
    (void)height;
        if (charts_page.list) MoveWindow(charts_page.list, 16, 184, std::max(360, width * 44 / 100), height - 204, TRUE);
        if (charts_page.analysis) MoveWindow(charts_page.analysis, width * 46 / 100, 184, width * 52 / 100 - 16, 245, TRUE);
        if (charts_page.chart_tabs) MoveWindow(charts_page.chart_tabs, width * 46 / 100, 435, width - width * 46 / 100 - 16, 30, TRUE);
}
bool Application::command_charts(WPARAM wparam) {
    switch(LOWORD(wparam)) {
        case ID_ADD: add_ticker(); return true;
        case ID_TARGET_SET: set_target(); return true;
        case ID_REMOVE: remove_selected(); return true;
        case ID_DIVIDENDS_OPEN: open_dividends_window(); return true;

        case ID_FILTER:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                charts_page.filter = static_cast<WatchFilter>(SendMessageW(GetDlgItem(app_window, ID_FILTER), CB_GETCURSEL, 0, 0));
                fill_list();
            }
            return true;
        case ID_RANGE:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                const int ranges[] = {22, 66, 252, 0};
                const auto index = SendMessageW(GetDlgItem(app_window, ID_RANGE), CB_GETCURSEL, 0, 0);
                if (index >= 0 && index < 4) charts_page.chart_sessions = ranges[index];
                InvalidateRect(app_window, nullptr, TRUE);
            }
            return true;
    }
    return false;
}

bool Application::notify_charts(LPARAM lparam) {
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_CHART_TABS && reinterpret_cast<NMHDR*>(lparam)->code == TCN_SELCHANGE) {
            charts_page.chart_panel = static_cast<ChartPanel>(std::clamp(TabCtrl_GetCurSel(charts_page.chart_tabs), 0, 3));
            InvalidateRect(app_window, nullptr, TRUE); return true;
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_LIST && reinterpret_cast<NMHDR*>(lparam)->code == LVN_ITEMCHANGED) {
            if (app_filling) return true;
            const int selected = selected_instrument();
            if (selected >= 0 && static_cast<std::size_t>(selected) < app_items.size()) { charts_page.selected = static_cast<std::size_t>(selected); update_analysis(); }
            return true;
        }

    return false;
}

void Application::paint_charts(HDC dc,RECT client) {
            RECT chart{client.left + client.right * 46 / 100, 475, client.right - 16, client.bottom - 20};
            const auto old_font = SelectObject(dc, app_font);
            draw_chart(dc, chart, charts_page.selected, charts_page.chart_panel, charts_page.chart_sessions);
            SelectObject(dc, old_font);
}

}
#endif
