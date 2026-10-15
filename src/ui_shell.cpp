#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
int Application::selected_instrument() {
    const auto selected=selected_row_data(charts_page.list);
    return selected && *selected<app_items.size()?static_cast<int>(*selected):-1;
}

void Application::storage_failure(const char* error) {
    app_storage_blocked = true;
    MessageBoxW(app_window, widen(std::string("Ошибка хранения данных: ") + error +
        "\nСделайте резервную копию. До исправления ошибки автоматическая запись отключена.").c_str(), L"MOEXWatch", MB_ICONERROR);
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

HWND Application::create_chart_tabs(HWND parent, int id, int x, int y, int width) {
    HWND tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_SINGLELINE,
        x, y, width, 30, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (app_font) SendMessageW(tabs, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
    const wchar_t* names[] = {L"Цена", L"Сравнение с IMOEX", L"Объёмы", L"RSI"};
    for (int i = 0; i < 4; ++i) {
        TCITEMW tab{}; tab.mask = TCIF_TEXT; tab.pszText = const_cast<wchar_t*>(names[i]);
        TabCtrl_InsertItem(tabs, i, &tab);
    }
    TabCtrl_SetCurSel(tabs, 0);
    return tabs;
}

void Application::draw_chart(HDC dc, RECT area, std::size_t instrument_index, ChartPanel panel, int sessions) {
    FillRect(dc, &area, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    SetBkMode(dc, TRANSPARENT);
    if (instrument_index >= app_items.size() || app_items[instrument_index].candles.empty()) {
        DrawTextW(dc, L"Нет данных для графика. Выберите акцию и нажмите «Обновить».", -1,
            &area, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }
    const auto& item = app_items[instrument_index];
    const auto& candles = item.candles;
    const std::size_t count = candles.size();
    const std::size_t first = sessions > 0 && count > static_cast<std::size_t>(sessions)
        ? count - static_cast<std::size_t>(sessions) : 0;
    const std::size_t shown = count - first;
    RECT plot{area.left + 80, area.top + (panel == ChartPanel::Price ? 76 : 58), area.right - 16, area.bottom - 28};
    if (plot.right - plot.left < 40 || plot.bottom - plot.top < 40) return;
    const int saved_dc = SaveDC(dc);
    IntersectClipRect(dc, area.left, area.top, area.right, area.bottom);
    const auto text = [&](int x, int y, const std::string& value, COLORREF color) {
        SetTextColor(dc, color);
        const auto label = widen(value);
        RECT line{x, y, area.right - 4, y + 21};
        DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &line, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    };
    const auto x_at = [&](std::size_t i) {
        return static_cast<int>(plot.left + (static_cast<double>(i - first) + 0.5) *
            static_cast<double>(plot.right - plot.left) / static_cast<double>(shown));
    };
    const COLORREF blue = RGB(34, 105, 181), orange = RGB(232, 139, 43), purple = RGB(135, 73, 170);
    const COLORREF gray = RGB(85, 95, 110);
    std::vector<std::optional<double>> primary, secondary, sma20, sma50, sma200;
    std::optional<PortfolioPosition> position;
    const PositionLevels* position_levels = nullptr;
    double minimum = 0.0, maximum = 1.0;
    std::string heading = item.ticker;
    if (item.candles_stale) heading += " | история устарела";

    if (panel == ChartPanel::Price) {
        heading += " | Дневные свечи";
        sma20 = sma_series(candles, 20); sma50 = sma_series(candles, 50); sma200 = sma_series(candles, 200);
        minimum = candles[first].low; maximum = candles[first].high;
        for (std::size_t i = first; i < count; ++i) {
            minimum = std::min(minimum, candles[i].low); maximum = std::max(maximum, candles[i].high);
            for (const auto* values : {&sma20, &sma50, &sma200}) {
                if ((*values)[i]) { minimum = std::min(minimum, *(*values)[i]); maximum = std::max(maximum, *(*values)[i]); }
            }
        }
        position = open_position_for_ticker(item.ticker);
        if (position) {
            position_levels = levels_for_position(position->ticker, position->opened_on);
            minimum = std::min(minimum, position->average_entry_price);
            maximum = std::max(maximum, position->average_entry_price);
            if (position_levels) {
                for (const auto level : {position_levels->stop_loss, position_levels->take_profit}) {
                    if (level) { minimum = std::min(minimum, *level); maximum = std::max(maximum, *level); }
                }
            }
        }
        text(area.left + 4, area.top + 22, "SMA20", orange);
        text(area.left + 92, area.top + 22, "SMA50", blue);
        text(area.left + 180, area.top + 22, "SMA200", purple);
        const auto atr = atr_series(candles).back();
        const auto atr_percent = atr && candles.back().close > 0.0
            ? std::optional<double>{*atr / candles.back().close * 100.0} : std::optional<double>{};
        std::string detail = "ATR(14): " + optional_number(atr) + " ₽ (" + percent(atr_percent) + ")";
        if (!item.dividends.empty()) detail += " | D: дата отсечки";
        text(area.left + 4, area.top + 44, detail, gray);
    } else if (panel == ChartPanel::Comparison) {
        heading += " | Сравнение с IMOEX, база 100";
        const auto comparison = normalized_comparison_series(candles, app_imoex_candles, first);
        primary = comparison.stock; secondary = comparison.benchmark;
        minimum = 100.0; maximum = 100.0;
        std::optional<double> stock_last, index_last;
        for (std::size_t i = first; i < count; ++i) {
            if (primary[i] && secondary[i]) {
                minimum = std::min({minimum, *primary[i], *secondary[i]});
                maximum = std::max({maximum, *primary[i], *secondary[i]});
                stock_last = *primary[i] - 100.0; index_last = *secondary[i] - 100.0;
            }
        }
        if (!stock_last) {
            text(area.left + 4, area.top, heading, gray);
            RECT message = plot;
            DrawTextW(dc, L"Нет общих дневных дат с IMOEX. Нажмите «Обновить» в основном окне.", -1,
                &message, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            RestoreDC(dc, saved_dc); return;
        }
        text(area.left + 4, area.top + 22, "Акция (синяя): " + percent(stock_last) + " | IMOEX (оранжевая): " + percent(index_last) +
            (app_imoex_stale ? " | IMOEX устарел" : ""), gray);
    } else if (panel == ChartPanel::Volume) {
        heading += " | Дневной объём, акций";
        secondary = volume_ratio_series(candles);
        maximum = 0.0;
        for (std::size_t i = first; i < count; ++i) maximum = std::max(maximum, candles[i].volume);
        maximum = maximum > 0.0 ? maximum * 1.1 : 1.0;
        text(area.left + 4, area.top + 22, "Последний: " + optional_number(secondary.back()) +
            "× среднего20 | оранжевый: >2× предыдущих 20 сессий", gray);
    } else {
        primary = rsi_series(candles);
        heading += " | RSI(14) Уайлдера: " + optional_number(primary.back());
        maximum = 100.0;
        text(area.left + 4, area.top + 22, "Ниже 30: перепроданность | выше 70: перекупленность", gray);
    }
    text(area.left + 4, area.top, heading, RGB(40, 50, 65));
    if (panel == ChartPanel::Price || panel == ChartPanel::Comparison) {
        const double padding = std::max((maximum - minimum) * 0.07, std::max(std::abs(maximum) * 0.001, 0.01));
        minimum -= padding; maximum += padding;
    }
    const auto y_at = [&](double value) {
        return static_cast<int>(plot.bottom - (value - minimum) / (maximum - minimum) *
            static_cast<double>(plot.bottom - plot.top));
    };
    if (panel == ChartPanel::Rsi) {
        RECT high_band{plot.left, plot.top, plot.right, y_at(70.0)};
        RECT low_band{plot.left, y_at(30.0), plot.right, plot.bottom};
        HBRUSH high_brush = CreateSolidBrush(RGB(255, 235, 236));
        HBRUSH low_brush = CreateSolidBrush(RGB(230, 247, 239));
        FillRect(dc, &high_band, high_brush); FillRect(dc, &low_band, low_brush);
        DeleteObject(high_brush); DeleteObject(low_brush);
    }
    HPEN grid = CreatePen(PS_SOLID, 1, RGB(225, 231, 238));
    HGDIOBJ old_pen = SelectObject(dc, grid);
    const double rsi_levels[] = {0.0, 30.0, 50.0, 70.0, 100.0};
    for (int k = 0; k <= 4; ++k) {
        const double value = panel == ChartPanel::Rsi ? rsi_levels[k] : minimum + (maximum - minimum) * static_cast<double>(k) / 4.0;
        const int y = y_at(value);
        MoveToEx(dc, plot.left, y, nullptr); LineTo(dc, plot.right, y);
        std::string label = format_price(value);
        if (panel == ChartPanel::Volume) {
            if (value >= 1e9) label = money(value / 1e9) + " млрд";
            else if (value >= 1e6) label = money(value / 1e6) + " млн";
            else if (value >= 1e3) label = money(value / 1e3) + " тыс";
        }
        text(area.left + 2, y - 8, label, gray);
    }
    SelectObject(dc, old_pen); DeleteObject(grid);
    const auto draw_series = [&](const std::vector<std::optional<double>>& values, COLORREF color) {
        HPEN pen = CreatePen(PS_SOLID, 2, color);
        HGDIOBJ previous = SelectObject(dc, pen);
        bool started = false;
        for (std::size_t i = first; i < count; ++i) {
            if (!values[i]) { started = false; continue; }
            const int x = x_at(i), y = y_at(*values[i]);
            if (started) LineTo(dc, x, y); else { MoveToEx(dc, x, y, nullptr); started = true; }
        }
        SelectObject(dc, previous); DeleteObject(pen);
    };
    const int half = std::clamp(static_cast<int>((plot.right - plot.left) / static_cast<LONG>(shown) / 3), 1, 10);
    if (panel == ChartPanel::Price || panel == ChartPanel::Volume) {
        for (std::size_t i = first; i < count; ++i) {
            const auto& candle = candles[i];
            const bool spike = panel == ChartPanel::Volume && secondary[i] && *secondary[i] > 2.0;
            const COLORREF color = spike ? orange : (candle.close >= candle.open ? RGB(29, 144, 100) : RGB(207, 66, 78));
            HPEN pen = CreatePen(PS_SOLID, 1, color); HBRUSH brush = CreateSolidBrush(color);
            HGDIOBJ previous_pen = SelectObject(dc, pen), previous_brush = SelectObject(dc, brush);
            const int x = x_at(i);
            if (panel == ChartPanel::Price) {
                MoveToEx(dc, x, y_at(candle.high), nullptr); LineTo(dc, x, y_at(candle.low));
                const int top = std::min(y_at(candle.open), y_at(candle.close));
                const int bottom = std::max(top + 1, std::max(y_at(candle.open), y_at(candle.close)));
                Rectangle(dc, x - half, top, x + half + 1, bottom + 1);
            } else {
                RECT bar{x - half, y_at(candle.volume), x + half + 1, plot.bottom};
                FillRect(dc, &bar, brush);
            }
            SelectObject(dc, previous_brush); SelectObject(dc, previous_pen); DeleteObject(brush); DeleteObject(pen);
        }
    }
    if (panel == ChartPanel::Price) {
        draw_series(sma20, orange); draw_series(sma50, blue); draw_series(sma200, purple);
        const auto draw_level = [&](double value, const std::string& label, COLORREF color) {
            HPEN pen = CreatePen(PS_DASH, 1, color); HGDIOBJ previous = SelectObject(dc, pen);
            const int y = y_at(value); MoveToEx(dc, plot.left, y, nullptr); LineTo(dc, plot.right, y);
            SelectObject(dc, previous); DeleteObject(pen);
            text(std::max(plot.left + 4, plot.right - 180), y - 19, label + " " + money(value), color);
        };
        if (position) {
            draw_level(position->average_entry_price, "Средняя", gray);
            if (position_levels && position_levels->stop_loss) draw_level(*position_levels->stop_loss, "Стоп", RGB(200, 55, 65));
            if (position_levels && position_levels->take_profit) draw_level(*position_levels->take_profit, "Тейк", RGB(25, 135, 90));
        }
        HPEN event_pen = CreatePen(PS_DOT, 1, RGB(190, 91, 40)); old_pen = SelectObject(dc, event_pen);
        for (const auto& event : item.dividends) {
            if (event.status == DividendStatus::Cancelled || event.verification == DividendVerification::Conflict) continue;
            for (std::size_t i = first; i < count; ++i) {
                if (candles[i].date.substr(0, 10) != event.record_date) continue;
                const int x = x_at(i); MoveToEx(dc, x, plot.top, nullptr); LineTo(dc, x, plot.bottom);
                text(x + 3, plot.top + 2, event.status == DividendStatus::Forecast ? "D?" : "D", RGB(160, 80, 30)); break;
            }
        }
        SelectObject(dc, old_pen); DeleteObject(event_pen);
    } else if (panel == ChartPanel::Comparison) {
        draw_series(primary, blue); draw_series(secondary, orange);
    } else if (panel == ChartPanel::Rsi) draw_series(primary, purple);
    text(plot.left, area.bottom - 22, candles[first].date.substr(0, 10), gray);
    text(plot.right - 98, area.bottom - 22, candles.back().date.substr(0, 10), gray);
    RestoreDC(dc, saved_dc);
}

void Application::fill_dividend_list() {
    if (!charts_page.dividend_list || charts_page.dividend_instrument >= app_items.size()) return;
    ListView_DeleteAllItems(charts_page.dividend_list);
    const auto& item = app_items[charts_page.dividend_instrument];
    for (std::size_t index = 0; index < item.dividends.size(); ++index) {
        const auto& event = item.dividends[index];
        const bool historical = !event.record_date.empty() && event.record_date < today_date();
        std::wstring yield = L"н/д";
        if (event.status != DividendStatus::Cancelled && event.verification != DividendVerification::Conflict) {
            if (!historical && item.quote.last && *item.quote.last > 0.0 && !item.quote_stale)
                yield = widen(percent(event.amount_per_share / *item.quote.last * 100.0) + " (к текущей)");
            else if (!event.last_buy_date.empty()) {
                const auto candle = std::find_if(item.candles.begin(), item.candles.end(), [&](const auto& c) { return c.date.substr(0,10) == event.last_buy_date; });
                if (candle != item.candles.end() && candle->close > 0.0 && !item.candles_stale)
                    yield = widen(percent(event.amount_per_share / candle->close * 100.0) + " (к закрытию)");
            }
        }
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

void Application::create_charts_controls(HWND window) {
    add_control(window, L"STATIC", L"Тикер", 0, 16, 61, 48, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 66, 57, 105, 28, ID_TICKER);
    charts_page.ticker_edit = GetDlgItem(window, ID_TICKER);
    add_control(window, L"BUTTON", L"Добавить", BS_PUSHBUTTON, 180, 57, 95, 28, ID_ADD);
    add_control(window, L"STATIC", L"Целевая цена, ₽", 0, 290, 61, 125, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 420, 57, 105, 28, ID_TARGET);
    charts_page.target_edit = GetDlgItem(window, ID_TARGET);
    add_control(window, L"BUTTON", L"Сохранить цель", BS_PUSHBUTTON, 535, 57, 125, 28, ID_TARGET_SET);
    add_control(window, L"BUTTON", L"Удалить акцию", BS_PUSHBUTTON, 670, 57, 115, 28, ID_REMOVE);
    add_control(window, L"BUTTON", L"Обновить", BS_PUSHBUTTON, 795, 57, 105, 28, ID_REFRESH);
    add_control(window, L"BUTTON", L"Дивиденды", BS_PUSHBUTTON, 914, 57, 125, 28, ID_DIVIDENDS_OPEN);
    add_control(window, L"STATIC", L"Данные MOEX ISS могут отображаться с задержкой до 15 минут. Это приложение не отправляет сделки.", 0, 16, 94, 950, 22, 0);
    charts_page.status = CreateWindowExW(0, L"STATIC", L"Готово", WS_CHILD | WS_VISIBLE, 16, 119, 940, 24, window, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (app_font) SendMessageW(charts_page.status, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
    charts_page.market_controls.push_back(charts_page.status);
    add_control(window, L"STATIC", L"Фильтр", 0, 16, 153, 58, 24, 0);
    add_control(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 78, 149, 265, 220, ID_FILTER);
    const wchar_t* filters[] = {L"Все акции", L"Перепроданность: RSI < 30", L"Перекупленность: RSI > 70", L"Цена выше SMA200", L"До цели не более 5%"};
    for (const auto* name : filters) SendMessageW(GetDlgItem(window, ID_FILTER), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    SendMessageW(GetDlgItem(window, ID_FILTER), CB_SETCURSEL, 0, 0);
    add_control(window, L"STATIC", L"График", 0, 375, 153, 60, 24, 0);
    add_control(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 440, 149, 260, 180, ID_RANGE);
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
        if(command_backtest(wparam)||command_signals(wparam)||command_portfolio(wparam)||command_trades(wparam))return 0;
        switch (LOWORD(wparam)) {
        case ID_ADD: add_ticker(); return 0;
        case ID_TARGET_SET: set_target(); return 0;
        case ID_REMOVE: remove_selected(); return 0;
        case ID_REFRESH: refresh_data(); return 0;
        case ID_BACKUP: start_backup(); return 0;
        case ID_DIVIDENDS_OPEN: open_dividends_window(); return 0;









        case ID_FILTER:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                charts_page.filter = static_cast<WatchFilter>(SendMessageW(GetDlgItem(window, ID_FILTER), CB_GETCURSEL, 0, 0));
                fill_list();
            }
            return 0;
        case ID_RANGE:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                const int ranges[] = {22, 66, 252, 0};
                const auto index = SendMessageW(GetDlgItem(window, ID_RANGE), CB_GETCURSEL, 0, 0);
                if (index >= 0 && index < 4) charts_page.chart_sessions = ranges[index];
                InvalidateRect(window, nullptr, TRUE);
            }
            return 0;
        }
        break;
    case WM_NOTIFY:
        if(reinterpret_cast<NMHDR*>(lparam)->code==LVN_COLUMNCLICK) {
            const auto* click=reinterpret_cast<NMLISTVIEW*>(lparam);const int id=static_cast<int>(click->hdr.idFrom);
            if(id==ID_LIST||id==ID_TRADE_LIST||id==ID_PORT_POSITIONS||id==ID_PORT_TRANSACTIONS||id==ID_SIG_LIST){auto& sort=app_table_sorts[id];if(sort.column==click->iSubItem)sort.ascending=!sort.ascending;else{sort.column=click->iSubItem;sort.ascending=true;}apply_table_sort(click->hdr.hwndFrom,id);if(id==ID_PORT_POSITIONS)fill_position_level_editor();return 0;}
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_CHART_TABS && reinterpret_cast<NMHDR*>(lparam)->code == TCN_SELCHANGE) {
            charts_page.chart_panel = static_cast<ChartPanel>(std::clamp(TabCtrl_GetCurSel(charts_page.chart_tabs), 0, 3));
            InvalidateRect(window, nullptr, TRUE); return 0;
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_TABS && reinterpret_cast<NMHDR*>(lparam)->code == TCN_SELCHANGE) {
            activate_page(TabCtrl_GetCurSel(app_tabs)); return 0;
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_LIST && reinterpret_cast<NMHDR*>(lparam)->code == LVN_ITEMCHANGED) {
            if (app_filling) return 0;
            const int selected = selected_instrument();
            if (selected >= 0 && static_cast<std::size_t>(selected) < app_items.size()) { charts_page.selected = static_cast<std::size_t>(selected); update_analysis(); }
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
            RECT chart{client.left + client.right * 46 / 100, 475, client.right - 16, client.bottom - 20};
            const auto old_font = SelectObject(dc, app_font);
            draw_chart(dc, chart, charts_page.selected, charts_page.chart_panel, charts_page.chart_sessions);
            SelectObject(dc, old_font);
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
        const wchar_t* headers[] = {L"Дата отсечки", L"Последний день покупки¹", L"На акцию", L"Доходность²", L"Статус", L"Сверка с эмитентом", L"Купить до (источник)", L"Источник", L"Эмитент", L"Основание расчёта¹"};
        const int widths[] = {115, 180, 125, 160, 230, 280, 170, 340, 340, 460};
        for (int i = 0; i < 10; ++i) {
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







void Application::resize_charts(int width, int height) {
    (void)height;
        if (charts_page.list) MoveWindow(charts_page.list, 16, 184, std::max(360, width * 44 / 100), height - 204, TRUE);
        if (charts_page.analysis) MoveWindow(charts_page.analysis, width * 46 / 100, 184, width * 52 / 100 - 16, 245, TRUE);
        if (charts_page.chart_tabs) MoveWindow(charts_page.chart_tabs, width * 46 / 100, 435, width - width * 46 / 100 - 16, 30, TRUE);
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
