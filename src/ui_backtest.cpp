#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
void Application::start_backtest() {
    bool expected = false;
    if (!backtest_page.busy.compare_exchange_strong(expected, true)) return;
    const auto selected = SendMessageW(backtest_page.ticker, CB_GETCURSEL, 0, 0);
    if (selected < 0 || static_cast<std::size_t>(selected) >= state.market.instruments.size()) {
        backtest_page.busy = false;
        MessageBoxW(shell.window, L"Сначала добавьте акцию в список наблюдения и обновите данные.", L"Тест стратегии", MB_ICONWARNING);
        return;
    }
    const auto capital = parse_finite_number(narrow(read_edit(backtest_page.capital)));
    const auto commission = parse_finite_number(narrow(read_edit(backtest_page.fee)));
    if (!capital || *capital <= 0.0 || !commission || *commission < 0.0 || *commission > 10.0) {
        backtest_page.busy = false;
        MessageBoxW(shell.window, L"Укажите капитал больше нуля и комиссию за одну сторону от 0 до 10%.", L"Проверьте параметры", MB_ICONWARNING);
        return;
    }
    const auto item = state.market.instruments[static_cast<std::size_t>(selected)];
    if (item.candles.empty() || item.candles_stale) {
        backtest_page.busy = false;
        MessageBoxW(shell.window, L"Сначала нажмите «Обновить» и дождитесь загрузки дневной истории выбранной акции.", L"Нет истории акции", MB_ICONWARNING);
        return;
    }
    EnableWindow(backtest_page.run, FALSE);
    SetWindowTextW(backtest_page.result, L"Загружаю историю IMOEX и рассчитываю результаты…");
    const auto settings = signals_page.settings;
    const auto target = item.target;
    const auto capital_value = *capital;
    const auto commission_value = *commission;
    try {auto context=fetch_context(0);
    tasks.start(TaskKind::Backtest,mailbox,[item,settings,target,capital_value,commission_value,context](std::stop_token stop) mutable -> BackgroundEvent {
        context.stop=stop;
        auto result = std::make_unique<BacktestUiResult>();
        try {
            const auto index = fetch_index_candles(context);
            const auto summary = run_backtest(item.candles, index, settings, target, capital_value, commission_value);
            if (!summary) throw std::runtime_error("Недостаточно общих торговых дат или заданы неверные параметры.");
            std::ostringstream out;
            out << std::fixed << std::setprecision(2)
                << "Акция: " << item.ticker << "    Сравнение: IMOEX (удержание)\r\n"
                << "Общий период: " << summary->start_date << " — " << summary->end_date << "    Начальный капитал: " << format_price(summary->starting_capital) << " ₽\r\n"
                << "Стратегия: " << format_price(summary->strategy_final_value) << " ₽  |  доходность " << summary->strategy_return_percent << "%  |  закрытых сделок: " << summary->completed_trades << "  |  макс. просадка: " << summary->strategy_max_drawdown_percent << "%\r\n"
                << "IMOEX:     " << format_price(summary->benchmark_final_value) << " ₽  |  доходность " << summary->benchmark_return_percent << "%\r\n\r\n"
                << "Комиссия " << commission_value << "% учтена при каждой покупке и продаже. Для обоих вариантов показана оценка после комиссии. Сигнал на закрытии исполняется по открытию следующей общей сессии.";
            check_cancelled(stop);
            result->text = out.str();
        } catch (const Cancelled&) {result->cancelled=true;result->text="Тест отменён.";} catch (const std::exception& ex) {
            result->failed = true;
            result->text = std::string("Не удалось выполнить тест: ") + ex.what();
        }
        return {0,std::move(*result)};
    });
    }catch(const std::exception&){backtest_page.busy=false;EnableWindow(backtest_page.run,TRUE);SetWindowTextW(backtest_page.result,L"Не удалось запустить тест.");}
    update_task_controls();
}

void Application::create_backtest_controls(HWND window) {
    add_control(backtest_page.controls,window, L"STATIC", L"Акция", 0, 16, 61, 55, 24, 0);
    backtest_page.ticker = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        78, 55, 145, 220, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BACKTEST_TICKER)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(backtest_page.ticker, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    backtest_page.controls.push_back(backtest_page.ticker);
    add_control(backtest_page.controls,window, L"STATIC", L"Капитал, ₽", 0, 242, 61, 85, 24, 0);
    add_control(backtest_page.controls,window, L"EDIT", L"100000", WS_BORDER | ES_AUTOHSCROLL, 330, 55, 110, 28, ID_BACKTEST_CAPITAL);
    backtest_page.capital = GetDlgItem(window, ID_BACKTEST_CAPITAL);
    add_control(backtest_page.controls,window, L"STATIC", L"Комиссия за сторону, %", 0, 458, 61, 168, 24, 0);
    add_control(backtest_page.controls,window, L"EDIT", L"0.05", WS_BORDER | ES_AUTOHSCROLL, 630, 55, 80, 28, ID_BACKTEST_FEE);
    backtest_page.fee = GetDlgItem(window, ID_BACKTEST_FEE);
    backtest_page.run = CreateWindowExW(0, L"BUTTON", L"Запустить тест", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        920, 55, 145, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BACKTEST_RUN)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(backtest_page.run, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    backtest_page.controls.push_back(backtest_page.run);
    add_control(backtest_page.controls,window, L"STATIC", L"Тестируются сохранённые условия со вкладки «Сигналы». Сделки — на следующем открытии после сигнала на закрытии.",
                0, 16, 96, 1190, 24, 0);
    backtest_page.result = CreateWindowExW(0, L"STATIC", L"Выберите акцию из списка наблюдения и нажмите «Запустить тест стратегии».",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 145, 1220, 200, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BACKTEST_RESULT)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(backtest_page.result, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    backtest_page.controls.push_back(backtest_page.result);

}

void Application::resize_backtest(int width, int height) {
    (void)height;
        if (backtest_page.result) MoveWindow(backtest_page.result, 16, 145, width - 32, 260, TRUE);
}

bool Application::command_backtest(WPARAM wparam) {
    switch(LOWORD(wparam)) {
        case ID_BACKTEST_RUN: start_backtest(); return true;
    }
    return false;
}

void Application::complete_backtest(LPARAM lparam) {
        std::unique_ptr<BacktestUiResult> result(reinterpret_cast<BacktestUiResult*>(lparam));
        backtest_page.busy = false;
        EnableWindow(backtest_page.run, TRUE);
    update_task_controls();
        const auto display = widen(result->text);
        SetWindowTextW(backtest_page.result, display.c_str());
}
} // namespace moexwatch::ui
#endif
