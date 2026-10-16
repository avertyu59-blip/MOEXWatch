#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
void Application::save_signal_settings() {
    if (state.persistence.writes_blocked) return;
    try { storage::save_signal_settings(storage::data_directory(), signals_page.settings); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void Application::load_signal_settings() {
    try { auto loaded = signals_page.settings; storage::load_signal_settings(storage::data_directory(), loaded); signals_page.settings = std::move(loaded); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void Application::update_auto_refresh_timer() {
    if (!shell.window) return;
    KillTimer(shell.window, TIMER_AUTO_REFRESH);
    if (signals_page.settings.auto_refresh_enabled) {
        const auto milliseconds = static_cast<UINT>(signals_page.settings.auto_refresh_minutes) * 60U * 1000U;
        SetTimer(shell.window, TIMER_AUTO_REFRESH, milliseconds, nullptr);
    }
}

void Application::synchronize_signal_controls() {
    CheckDlgButton(shell.window, ID_SIG_OVERSOLD, signals_page.settings.rsi_oversold_enabled ? BST_CHECKED : BST_UNCHECKED);
    SetWindowTextW(signals_page.oversold_value, widen(money(signals_page.settings.rsi_oversold_below)).c_str());
    CheckDlgButton(shell.window, ID_SIG_OVERBOUGHT, signals_page.settings.rsi_overbought_enabled ? BST_CHECKED : BST_UNCHECKED);
    SetWindowTextW(signals_page.overbought_value, widen(money(signals_page.settings.rsi_overbought_above)).c_str());
    CheckDlgButton(shell.window, ID_SIG_SMA, signals_page.settings.above_sma200_enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(shell.window, ID_SIG_TARGET, signals_page.settings.near_target_enabled ? BST_CHECKED : BST_UNCHECKED);
    SetWindowTextW(signals_page.target_value, widen(money(signals_page.settings.near_target_percent)).c_str());
    SendMessageW(GetDlgItem(shell.window, ID_SIG_MODE), CB_SETCURSEL, signals_page.settings.require_all ? 1 : 0, 0);
    CheckDlgButton(shell.window, ID_SIG_NOTIFY, signals_page.settings.notifications_enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(shell.window, ID_SIG_AUTO_REFRESH, signals_page.settings.auto_refresh_enabled ? BST_CHECKED : BST_UNCHECKED);
    const int interval_index = signals_page.settings.auto_refresh_minutes == 30 ? 1 :
        (signals_page.settings.auto_refresh_minutes == 60 ? 2 : 0);
    SendMessageW(signals_page.interval, CB_SETCURSEL, interval_index, 0);
}

bool Application::read_signal_settings_from_controls() {
    SignalSettings settings;
    settings.rsi_oversold_enabled = IsDlgButtonChecked(shell.window, ID_SIG_OVERSOLD) == BST_CHECKED;
    settings.rsi_overbought_enabled = IsDlgButtonChecked(shell.window, ID_SIG_OVERBOUGHT) == BST_CHECKED;
    settings.above_sma200_enabled = IsDlgButtonChecked(shell.window, ID_SIG_SMA) == BST_CHECKED;
    settings.near_target_enabled = IsDlgButtonChecked(shell.window, ID_SIG_TARGET) == BST_CHECKED;
    settings.notifications_enabled = IsDlgButtonChecked(shell.window, ID_SIG_NOTIFY) == BST_CHECKED;
    settings.auto_refresh_enabled = IsDlgButtonChecked(shell.window, ID_SIG_AUTO_REFRESH) == BST_CHECKED;
    const int interval_index = static_cast<int>(SendMessageW(signals_page.interval, CB_GETCURSEL, 0, 0));
    const int intervals[] = {15, 30, 60};
    if (interval_index < 0 || interval_index >= 3) {
        MessageBoxW(shell.window, L"Выберите интервал автообновления.", L"Проверьте настройки", MB_ICONWARNING);
        return false;
    }
    settings.auto_refresh_minutes = intervals[interval_index];
    settings.require_all = SendMessageW(GetDlgItem(shell.window, ID_SIG_MODE), CB_GETCURSEL, 0, 0) == 1;
    const auto oversold = parse_finite_number(narrow(read_edit(signals_page.oversold_value)));
    const auto overbought = parse_finite_number(narrow(read_edit(signals_page.overbought_value)));
    const auto near_target = parse_finite_number(narrow(read_edit(signals_page.target_value)));
    if (!oversold || !overbought || !near_target) {
        MessageBoxW(shell.window, L"Введите числовые пороги RSI и расстояния до цели.", L"Проверьте условия", MB_ICONWARNING);
        return false;
    }
    settings.rsi_oversold_below = *oversold;
    settings.rsi_overbought_above = *overbought;
    settings.near_target_percent = *near_target;
    if (!valid_signal_settings(settings)) {
        MessageBoxW(shell.window, L"Пороги RSI и расстояние до цели должны быть от 0 до 100.", L"Проверьте условия", MB_ICONWARNING);
        return false;
    }
    const bool changed=!same_technical_rules(signals_page.settings,settings);
    signals_page.settings = settings;
    save_signal_settings();
    update_auto_refresh_timer();
    if(changed)signals_page.baseline_ready = false;
    fill_signals();
    return true;
}

void Application::fill_signals() {
    if (!signals_page.list || !signals_page.summary) return;
    const ScopedViewUpdate update(signals_page.updating);
    ListView_DeleteAllItems(signals_page.list);
    std::size_t count = 0;
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for (const auto& item : state.market.instruments) {
        const auto analysis = analyze(item.candles);
        const auto evaluation = evaluate_signals(analysis, item.quote.last, item.target,
            item.quote_stale || !fresh_moex_quote(item.quote,now), item.candles_stale, signals_page.settings);
        if (!evaluation.matched) continue;
        ++count;
        LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(&item-state.market.instruments.data()); row.iItem = static_cast<int>(count - 1);
        auto ticker = widen(item.ticker); row.pszText = ticker.data();
        ListView_InsertItem(signals_page.list, &row);
        std::string reasons;
        for (std::size_t i = 0; i < evaluation.reasons.size(); ++i) {
            if (i) reasons += "; ";
            reasons += evaluation.reasons[i];
        }
        const std::string values[] = {optional_number(item.quote.last), optional_number(analysis.rsi14),
            optional_number(analysis.sma200), optional_number(item.target), reasons};
        for (int col = 1; col <= 5; ++col) {
            auto wide = widen(values[col - 1]); ListView_SetItemText(signals_page.list, static_cast<int>(count - 1), col, wide.data());
        }
    }
    const auto summary = L"Совпадений: " + std::to_wstring(count) +
        (signals_page.settings.require_all ? L"   Режим: все условия   " : L"   Режим: любое условие   ") +
        L"Котировка не старше 20 минут (МСК) и обновлённая история.";
    SetWindowTextW(signals_page.summary, summary.c_str());
    apply_table_sort(signals_page.list,ID_SIG_LIST);
}

void Application::add_notification_icon() {
    signals_page.notification_icon = {};
    signals_page.notification_icon.cbSize = sizeof(signals_page.notification_icon);
    signals_page.notification_icon.hWnd = shell.window;
    signals_page.notification_icon.uID = 1;
    signals_page.notification_icon.uFlags = NIF_ICON | NIF_TIP;
    signals_page.notification_icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    const wchar_t tip[] = L"MOEX Watch — наблюдение за сигналами";
    std::copy_n(tip, std::min(std::size(tip), std::size(signals_page.notification_icon.szTip)) - 1, signals_page.notification_icon.szTip);
    signals_page.notification_icon.szTip[std::size(signals_page.notification_icon.szTip) - 1] = L'\0';
    signals_page.notification_icon_ready = Shell_NotifyIconW(NIM_ADD, &signals_page.notification_icon) != FALSE;
}

std::string Application::signal_reason_text(const SignalEvaluation& evaluation) {
    std::string text;
    for (std::size_t i = 0; i < evaluation.reasons.size(); ++i) {
        if (i) text += "; ";
        text += evaluation.reasons[i];
    }
    return text;
}

void Application::notify_new_signals() {
    if(!signals_page.baseline_ready){signals_page.observer.reset_technical();signals_page.baseline_ready=true;}
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::vector<std::pair<std::string,std::string>> newly_matched;
    std::size_t fresh_count=0;
    for(const auto& item:state.market.instruments) {
        const auto evaluation=evaluate_signals(analyze(item.candles),item.quote.last,item.target,item.quote_stale,item.candles_stale,signals_page.settings);
        const bool fresh=!item.quote_stale && fresh_moex_quote(item.quote,now);if(fresh)++fresh_count;
        auto events=signals_page.observer.observe(item.quote,fresh,!item.candles_stale,evaluation,signals_page.settings.require_all?"Все условия":"Любое условие",now);
        for(auto& event:events){newly_matched.emplace_back(event.ticker,event.condition+"; цена "+format_price(event.price)+"; котировка "+event.quote_time);signals_page.history.push_back(std::move(event));signals_page.history_dirty=true;}
    }
    if(signals_page.history.size()>2000)signals_page.history.erase(signals_page.history.begin(),signals_page.history.end()-2000);
    signals_page.observation_status="Свежих котировок для проверки: "+std::to_string(fresh_count)+"/"+std::to_string(state.market.instruments.size())+" (возраст до 20 минут, МСК). История не зависит от всплывающих уведомлений.";
    if(signals_page.history_dirty && signals_page.observation_writable)try {
        storage::atomic_write(storage::data_directory()/"signal_history.csv",serialize_signal_history(signals_page.history));signals_page.history_dirty=false;signals_page.observation_error.clear();
    }catch(const std::exception& ex){signals_page.observation_error=std::string("Новые события пока только в памяти: ")+ex.what();}
    refresh_observation_page(signals_page.observation_page);
    if (!signals_page.settings.notifications_enabled || newly_matched.empty()) return;
    if (signals_page.notification_icon_ready) {
        std::wstring message;
        for (const auto& [ticker, reason] : newly_matched) {
            const std::wstring line = widen(ticker + ": " + reason);
            if (!message.empty()) message += L"\n";
            if (message.size() + line.size() >= std::size(signals_page.notification_icon.szInfo)) {
                if(message.empty())message=line.substr(0,std::size(signals_page.notification_icon.szInfo)-1);
                break;
            }
            message += line;
        }
        signals_page.notification_icon.uFlags = NIF_INFO;
        signals_page.notification_icon.dwInfoFlags = NIIF_INFO;
        const wchar_t title[] = L"Новый сигнал MOEX Watch";
        std::copy_n(title, std::min(std::size(title), std::size(signals_page.notification_icon.szInfoTitle)) - 1,
                    signals_page.notification_icon.szInfoTitle);
        signals_page.notification_icon.szInfoTitle[std::size(signals_page.notification_icon.szInfoTitle) - 1] = L'\0';
        const auto amount = std::min(message.size(), std::size(signals_page.notification_icon.szInfo) - 1);
        std::copy_n(message.begin(), amount, signals_page.notification_icon.szInfo);
        signals_page.notification_icon.szInfo[amount] = L'\0';
        if (Shell_NotifyIconW(NIM_MODIFY, &signals_page.notification_icon) == FALSE) FlashWindow(shell.window, TRUE);
    } else {
        FlashWindow(shell.window, TRUE);
        MessageBeep(MB_ICONINFORMATION);
    }
}

void Application::load_observations() {
    const auto failed=[this](const std::exception& ex){signals_page.observation_writable=false;if(!signals_page.observation_error.empty())signals_page.observation_error+="; ";signals_page.observation_error+=std::string(ex.what())+". Исходный файл сохранён; новые события только в памяти.";};
    try{if(const auto bytes=storage::read_text(storage::data_directory()/"price_alerts.csv"))signals_page.observer.configure(parse_price_alerts(*bytes));}catch(const std::exception& ex){failed(ex);}
    try{if(const auto bytes=storage::read_text(storage::data_directory()/"signal_history.csv"))signals_page.history=parse_signal_history(*bytes);}catch(const std::exception& ex){failed(ex);}
    refresh_observation_page(signals_page.observation_page);
}

void Application::create_signals_controls(HWND window) {
    add_control(signals_page.controls,window, L"BUTTON", L"RSI ниже", BS_AUTOCHECKBOX, 16, 59, 100, 28, ID_SIG_OVERSOLD);
    add_control(signals_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 120, 55, 65, 28, ID_SIG_OVERSOLD_VALUE);
    signals_page.oversold_value = GetDlgItem(window, ID_SIG_OVERSOLD_VALUE);
    add_control(signals_page.controls,window, L"BUTTON", L"RSI выше", BS_AUTOCHECKBOX, 215, 59, 100, 28, ID_SIG_OVERBOUGHT);
    add_control(signals_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 319, 55, 65, 28, ID_SIG_OVERBOUGHT_VALUE);
    signals_page.overbought_value = GetDlgItem(window, ID_SIG_OVERBOUGHT_VALUE);
    add_control(signals_page.controls,window, L"BUTTON", L"Цена выше SMA200", BS_AUTOCHECKBOX, 16, 97, 175, 28, ID_SIG_SMA);
    add_control(signals_page.controls,window, L"BUTTON", L"Цена рядом с целью", BS_AUTOCHECKBOX, 205, 97, 185, 28, ID_SIG_TARGET);
    add_control(signals_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 397, 95, 65, 28, ID_SIG_TARGET_VALUE);
    signals_page.target_value = GetDlgItem(window, ID_SIG_TARGET_VALUE);
    add_control(signals_page.controls,window, L"STATIC", L"%", 0, 466, 98, 20, 24, 0);
    add_control(signals_page.controls,window, L"STATIC", L"Совпадение", 0, 495, 98, 90, 24, 0);
    add_control(signals_page.controls,window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 585, 94, 135, 100, ID_SIG_MODE);
    SendMessageW(GetDlgItem(window, ID_SIG_MODE), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Любое условие"));
    SendMessageW(GetDlgItem(window, ID_SIG_MODE), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Все условия"));
    add_control(signals_page.controls,window, L"BUTTON", L"Всплывающие уведомления", BS_AUTOCHECKBOX, 738, 97, 240, 28, ID_SIG_NOTIFY);
    add_control(signals_page.controls,window, L"BUTTON", L"Сохранить условия", BS_PUSHBUTTON, 990, 95, 145, 28, ID_SIG_SAVE);
    add_control(signals_page.controls,window, L"BUTTON", L"Автообновление", BS_AUTOCHECKBOX, 16, 135, 155, 28, ID_SIG_AUTO_REFRESH);
    add_control(signals_page.controls,window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 176, 132, 135, 100, ID_SIG_INTERVAL);
    signals_page.interval = GetDlgItem(window, ID_SIG_INTERVAL);
    const wchar_t* interval_names[] = {L"15 минут", L"30 минут", L"60 минут"};
    for (const auto* interval : interval_names)
        SendMessageW(signals_page.interval, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(interval));
    add_control(signals_page.controls,window, L"STATIC", L"Проверка работает, пока приложение открыто; уведомления появляются при новых совпадениях.",
                0, 326, 137, 800, 22, 0);
    signals_page.summary = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 170, 1200, 25, window,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SIG_SUMMARY)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(signals_page.summary, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    signals_page.controls.push_back(signals_page.summary);
    signals_page.list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                    16, 202, 1260, 600, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SIG_LIST)), GetModuleHandleW(nullptr), nullptr);
    signals_page.controls.push_back(signals_page.list);
    ListView_SetExtendedListViewStyle(signals_page.list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* signal_headers[] = {L"Тикер", L"Цена, ₽", L"RSI14", L"SMA200, ₽", L"Цель, ₽", L"Почему совпало"};
    const int signal_widths[] = {90, 100, 90, 120, 100, 520};
    for (int i = 0; i < 6; ++i) { LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = const_cast<wchar_t*>(signal_headers[i]); col.cx = signal_widths[i]; ListView_InsertColumn(signals_page.list, i, &col); }
    if (shell.font) SendMessageW(signals_page.list, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);

}

void Application::create_observation_controls(HWND window) {
    ObservationBindings bindings;
    bindings.rules=[this] {return signals_page.observer.rules();};bindings.events=[this] {return signals_page.history;};
    bindings.tickers=[this] {std::vector<std::string> tickers;for(const auto& item:state.market.instruments)tickers.push_back(item.ticker);return tickers;};
    bindings.error=[this] {return signals_page.observation_status+" "+signals_page.observation_error;};
    bindings.save_rules=[this](const auto& rules){
        if(!signals_page.observation_writable)throw std::runtime_error("Сначала восстановите повреждённые файлы наблюдений из резервной копии");
        storage::atomic_write(storage::data_directory()/"price_alerts.csv",serialize_price_alerts(rules));signals_page.observer.configure(rules);
    };
    bindings.open_ticker=[this](const std::string& ticker){const auto it=std::find_if(state.market.instruments.begin(),state.market.instruments.end(),[&](const auto& item){return item.ticker==ticker;});if(it==state.market.instruments.end()){MessageBoxW(shell.window,L"Бумаги нет в наблюдении. Добавьте тикер на вкладке «Аналитика».",L"История сигналов",MB_ICONINFORMATION);return;}select_instrument(ticker,true);TabCtrl_SetCurSel(shell.tabs,0);activate_page(0);};
    signals_page.observation_page=create_observation_page(window,shell.font,std::move(bindings));
    if(!signals_page.observation_page)MessageBoxW(window,L"Не удалось создать вкладку истории сигналов.",L"MOEXWatch",MB_ICONERROR);

}

void Application::resize_signals(int width, int height) {
    (void)height;
        if (signals_page.summary) MoveWindow(signals_page.summary, 16, 170, width - 32, 25, TRUE);
        if (signals_page.list) MoveWindow(signals_page.list, 16, 202, width - 32, height - 222, TRUE);
}

bool Application::command_signals(WPARAM wparam) {
    switch(LOWORD(wparam)) {
        case ID_SIG_SAVE:
            if (read_signal_settings_from_controls()) MessageBoxW(shell.window, L"Условия сохранены. Уведомления будут приходить при появлении новых совпадений; текущие сигналы не дублируются.", L"Сигналы", MB_ICONINFORMATION);
            return true;
    }
    return false;
}
}
#endif
