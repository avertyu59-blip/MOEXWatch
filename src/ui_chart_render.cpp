#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
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
}
#endif
