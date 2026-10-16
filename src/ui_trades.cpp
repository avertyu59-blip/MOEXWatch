#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
void Application::fill_trades() {
    if (!trades_page.list) return;
    const ScopedViewUpdate update(trades_page.updating);
    ListView_DeleteAllItems(trades_page.list);
    const auto totals = calculate_trade_totals(trades_page.records);
    for (std::size_t i = 0; i < trades_page.records.size(); ++i) {
        const auto& trade = trades_page.records[i];
        const auto result = calculate_trade(trade);
        if (!result) continue;
        LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(i); row.iItem = static_cast<int>(i);
        auto ticker = widen(trade.ticker); row.pszText = ticker.data();
        ListView_InsertItem(trades_page.list, &row);
        const std::string values[] = {money(trade.quantity), money(trade.buy_price), money(trade.sell_price),
            money(result->gross_profit), money(trade.commission), percent(trade.tax_rate_percent),
            money(result->tax), money(result->net_profit)};
        for (int col = 1; col <= 8; ++col) {
            auto wide = widen(values[col - 1]); ListView_SetItemText(trades_page.list, static_cast<int>(i), col, wide.data());
        }
    }
    if (!totals) { SetWindowTextW(trades_page.summary, L"Расчёт невозможен: проверьте операции и диапазон чисел."); return; }
    const auto summary = L"Сделок: " + std::to_wstring(trades_page.records.size()) +
        L"     Валовая прибыль: " + widen(money(totals->gross_profit)) + L" ₽" +
        L"     После комиссий: " + widen(money(totals->profit_after_commission)) + L" ₽" +
        L"     Налог (оценка): " + widen(money(totals->tax)) + L" ₽" +
        L"     Чистая прибыль: " + widen(money(totals->net_profit)) + L" ₽";
    SetWindowTextW(trades_page.summary, summary.c_str());
    apply_table_sort(trades_page.list,ID_TRADE_LIST);
}

void Application::add_trade() {
    std::string ticker = narrow(read_edit(trades_page.ticker));
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    const auto quantity = parse_finite_number(narrow(read_edit(trades_page.quantity)));
    const auto buy = parse_finite_number(narrow(read_edit(trades_page.buy)));
    const auto sell = parse_finite_number(narrow(read_edit(trades_page.sell)));
    const auto commission_text = narrow(read_edit(trades_page.commission));
    const auto tax_text = narrow(read_edit(trades_page.tax));
    const auto commission = commission_text.empty() ? std::optional<double>(0.0) : parse_finite_number(commission_text);
    const auto tax = tax_text.empty() ? std::optional<double>(0.0) : parse_finite_number(tax_text);
    if (!quantity || !buy || !sell || !commission || !tax) {
        MessageBoxW(shell.window, L"Проверьте числа. Можно использовать точку или запятую; пустые комиссия и налог считаются равными 0.",
                    L"Не удалось добавить сделку", MB_ICONWARNING);
        return;
    }
    Trade trade{ticker, *quantity, *buy, *sell, *commission, *tax};
    if (!valid_trade(trade)) {
        MessageBoxW(shell.window, L"Укажите корректный тикер, количество и цены больше нуля, комиссию от 0 ₽ и ставку налога от 0 до 100%.",
                    L"Не удалось добавить сделку", MB_ICONWARNING);
        return;
    }
    trades_page.records.push_back(std::move(trade));
    if(!state.persistence.writes_blocked)storage_operation([&]{app_storage.save_trades(trades_page.records);}); fill_trades();
    SetWindowTextW(trades_page.ticker, L""); SetWindowTextW(trades_page.quantity, L"");
    SetWindowTextW(trades_page.buy, L""); SetWindowTextW(trades_page.sell, L"");
    SetWindowTextW(trades_page.commission, L""); SetWindowTextW(trades_page.tax, L"");
    SetFocus(trades_page.ticker);
}

void Application::delete_trade() {
    const auto selected_id=selected_row_data(trades_page.list);
    const int selected=selected_id?static_cast<int>(*selected_id):-1;
    if (selected < 0 || static_cast<std::size_t>(selected) >= trades_page.records.size()) return;
    trades_page.records.erase(trades_page.records.begin() + selected);
    if(!state.persistence.writes_blocked)storage_operation([&]{app_storage.save_trades(trades_page.records);}); fill_trades();
}

void Application::create_trades_controls(HWND window) {
    add_control(trades_page.controls,window, L"STATIC", L"Тикер", 0, 16, 59, 45, 24, 0);
    add_control(trades_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 64, 55, 100, 28, ID_TRADE_TICKER);
    trades_page.ticker = GetDlgItem(window, ID_TRADE_TICKER);
    add_control(trades_page.controls,window, L"STATIC", L"Количество", 0, 180, 59, 65, 24, 0);
    add_control(trades_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 248, 55, 95, 28, ID_TRADE_QUANTITY);
    trades_page.quantity = GetDlgItem(window, ID_TRADE_QUANTITY);
    add_control(trades_page.controls,window, L"STATIC", L"Покупка, ₽", 0, 360, 59, 75, 24, 0);
    add_control(trades_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 439, 55, 100, 28, ID_TRADE_BUY);
    trades_page.buy = GetDlgItem(window, ID_TRADE_BUY);
    add_control(trades_page.controls,window, L"STATIC", L"Продажа, ₽", 0, 555, 59, 75, 24, 0);
    add_control(trades_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 635, 55, 100, 28, ID_TRADE_SELL);
    trades_page.sell = GetDlgItem(window, ID_TRADE_SELL);
    add_control(trades_page.controls,window, L"STATIC", L"Комиссия, ₽", 0, 16, 99, 100, 24, 0);
    add_control(trades_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 118, 95, 105, 28, ID_TRADE_COMMISSION);
    trades_page.commission = GetDlgItem(window, ID_TRADE_COMMISSION);
    add_control(trades_page.controls,window, L"STATIC", L"Налоговая ставка, %", 0, 238, 99, 175, 24, 0);
    add_control(trades_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 430, 95, 90, 28, ID_TRADE_TAX);
    trades_page.tax = GetDlgItem(window, ID_TRADE_TAX);
    add_control(trades_page.controls,window, L"BUTTON", L"Записать сделку", BS_PUSHBUTTON, 540, 95, 140, 28, ID_TRADE_ADD);
    add_control(trades_page.controls,window, L"BUTTON", L"Удалить выбранную", BS_PUSHBUTTON, 692, 95, 155, 28, ID_TRADE_DELETE);
    add_control(trades_page.controls,window, L"STATIC", L"Налог рассчитывается ориентировочно по введённой ставке; пустое поле означает 0%. Проверьте ставку для вашей ситуации.",
                0, 16, 130, 1200, 22, 0);
    trades_page.summary = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 153, 1200, 26, window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TRADE_SUMMARY)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(trades_page.summary, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    trades_page.controls.push_back(trades_page.summary);
    trades_page.list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                   16, 184, 1260, 620, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TRADE_LIST)), GetModuleHandleW(nullptr), nullptr);
    trades_page.controls.push_back(trades_page.list);
    ListView_SetExtendedListViewStyle(trades_page.list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* trade_headers[] = {L"Тикер", L"Кол-во", L"Покупка, ₽", L"Продажа, ₽", L"Валовая, ₽", L"Комиссия, ₽", L"Налог, %", L"Налог, ₽", L"Чистая, ₽"};
    const int trade_widths[] = {75, 75, 105, 105, 110, 110, 90, 100, 110};
    for (int i = 0; i < 9; ++i) { LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = const_cast<wchar_t*>(trade_headers[i]); col.cx = trade_widths[i]; ListView_InsertColumn(trades_page.list, i, &col); }
    if (shell.font) SendMessageW(trades_page.list, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);

}

void Application::resize_trades(int width, int height) {
    (void)height;
        if (trades_page.summary) MoveWindow(trades_page.summary, 16, 153, width - 32, 26, TRUE);
        if (trades_page.list) MoveWindow(trades_page.list, 16, 184, width - 32, height - 204, TRUE);
}

bool Application::command_trades(WPARAM wparam) {
    switch(LOWORD(wparam)) {
        case ID_TRADE_ADD: add_trade(); return true;
        case ID_TRADE_DELETE: delete_trade(); return true;
    }
    return false;
}
}
#endif
