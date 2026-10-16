#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
void Application::save_portfolio() {
    if (state.persistence.writes_blocked) return;
    try { storage::save_portfolio(storage::data_directory(), portfolio_page.transactions); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void Application::load_portfolio() {
    try { auto loaded = portfolio_page.transactions; storage::load_portfolio(storage::data_directory(), loaded); portfolio_page.transactions = std::move(loaded); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void Application::save_position_levels() {
    if (state.persistence.writes_blocked) return;
    try { storage::save_position_levels(storage::data_directory(), portfolio_page.position_levels); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void Application::load_position_levels() {
    try { auto loaded = portfolio_page.position_levels; storage::load_position_levels(storage::data_directory(), loaded); portfolio_page.position_levels = std::move(loaded); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

std::vector<Quote> Application::portfolio_quotes() {
    std::vector<Quote> quotes;
    quotes.reserve(state.market.instruments.size());
    for (const auto& item : state.market.instruments) quotes.push_back(item.quote);
    return quotes;
}

const PositionLevels* Application::levels_for_position(std::string_view ticker, std::string_view opened_on) {
    const auto found = std::find_if(portfolio_page.position_levels.begin(), portfolio_page.position_levels.end(), [&](const auto& levels) {
        return levels.ticker == ticker && levels.opened_on == opened_on;
    });
    return found == portfolio_page.position_levels.end() ? nullptr : &*found;
}

std::optional<PortfolioPosition> Application::open_position_for_ticker(std::string_view ticker) {
    const auto portfolio = calculate_portfolio(portfolio_page.transactions, {});
    if (!portfolio) return std::nullopt;
    const auto found = std::find_if(portfolio->positions.begin(), portfolio->positions.end(), [&](const auto& position) {
        return position.ticker == ticker && position.quantity > 0.0;
    });
    return found == portfolio->positions.end() ? std::nullopt : std::optional<PortfolioPosition>(*found);
}

std::string Application::portfolio_side_name(PortfolioSide side) {
    return side == PortfolioSide::Buy ? "Покупка" : "Продажа";
}

void Application::fill_portfolio() {
    if (!portfolio_page.positions || !portfolio_page.transactions_list || !portfolio_page.summary) return;
    const ScopedViewUpdate update(portfolio_page.updating);
    ListView_DeleteAllItems(portfolio_page.positions);
    ListView_DeleteAllItems(portfolio_page.transactions_list);
    const auto portfolio = calculate_portfolio(portfolio_page.transactions, portfolio_quotes());
    if (portfolio) {
        const auto old_size = portfolio_page.position_levels.size();
        std::erase_if(portfolio_page.position_levels, [&](const auto& levels) {
            return std::none_of(portfolio->positions.begin(), portfolio->positions.end(), [&](const auto& position) {
                return position.quantity > 0.0 && position.ticker == levels.ticker && position.opened_on == levels.opened_on;
            });
        });
        if (portfolio_page.position_levels.size() != old_size) save_position_levels();
    }
    const bool allocation_fresh = portfolio && portfolio->priced_positions == portfolio->open_positions &&
        std::all_of(portfolio->positions.begin(),portfolio->positions.end(),[this](const auto& position) {
            if(position.quantity<=0.0)return true;
            const auto source=std::find_if(state.market.instruments.begin(),state.market.instruments.end(),[&](const auto& item){return item.ticker==position.ticker;});
            return source!=state.market.instruments.end() && !source->quote_stale;
        });
    std::size_t row_index = 0;
    if (portfolio) {
        for (const auto& position : portfolio->positions) {
            if (position.quantity <= 0.0) continue;
            LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(row_index); row.iItem = static_cast<int>(row_index);
            auto ticker = widen(position.ticker); row.pszText = ticker.data();
            ListView_InsertItem(portfolio_page.positions, &row);
            bool stale = false;
            const auto source = std::find_if(state.market.instruments.begin(), state.market.instruments.end(), [&](const auto& item) { return item.ticker == position.ticker; });
            if (source != state.market.instruments.end()) stale = source->quote_stale;
            const auto* levels = levels_for_position(position.ticker, position.opened_on);
            const std::string values[] = {money(position.quantity), money(position.average_entry_price),
                position.current_price ? money(*position.current_price) + (stale ? " *" : "") : "—",
                position.market_value ? money(*position.market_value) : "—",
                percent(portfolio_weight(*portfolio,position,allocation_fresh)),
                position.unrealized_profit ? money(*position.unrealized_profit) : "—",
                percent(position.unrealized_percent), money(position.realized_profit),
                levels && levels->stop_loss ? money(*levels->stop_loss) : "—",
                levels && levels->take_profit ? money(*levels->take_profit) : "—"};
            for (int col = 1; col <= 10; ++col) {
                auto wide = widen(values[col - 1]); ListView_SetItemText(portfolio_page.positions, static_cast<int>(row_index), col, wide.data());
            }
            ++row_index;
        }
    }
    for (std::size_t i = 0; i < portfolio_page.transactions.size(); ++i) {
        const auto& transaction = portfolio_page.transactions[i];
        LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(i); row.iItem = static_cast<int>(i);
        auto date = widen(transaction.date); row.pszText = date.data();
        ListView_InsertItem(portfolio_page.transactions_list, &row);
        const std::string values[] = {transaction.ticker, portfolio_side_name(transaction.side),
            money(transaction.quantity), money(transaction.price), money(transaction.commission)};
        for (int col = 1; col <= 5; ++col) {
            auto wide = widen(values[col - 1]); ListView_SetItemText(portfolio_page.transactions_list, static_cast<int>(i), col, wide.data());
        }
    }
    apply_table_sort(portfolio_page.positions,ID_PORT_POSITIONS);
    apply_table_sort(portfolio_page.transactions_list,ID_PORT_TRANSACTIONS);
    if (!portfolio) {
        SetWindowTextW(portfolio_page.summary, L"В журнале есть продажа без достаточного остатка. Проверьте и удалите ошибочную операцию.");
        return;
    }
    const auto summary = L"Открытых позиций: " + std::to_wstring(portfolio->open_positions) +
        L"   Котировки: " + std::to_wstring(portfolio->priced_positions) + L"/" + std::to_wstring(portfolio->open_positions) +
        (allocation_fresh ? L"   Стоимость: " : L"   Стоимость оценённых позиций*: ") + widen(money(portfolio->total_market_value)) + L" ₽" +
        L"   Бумажная прибыль: " + widen(money(portfolio->total_unrealized_profit)) + L" ₽" +
        L"   Зафиксировано: " + widen(money(portfolio->total_realized_profit)) + (allocation_fresh ? L" ₽" : L" ₽ (* — неполная/устаревшая оценка; доли н/д)");
    SetWindowTextW(portfolio_page.summary, summary.c_str());
}

void Application::fill_position_level_editor() {
    if (!portfolio_page.position_stop || !portfolio_page.position_take || !portfolio_page.positions) return;
    const int selected = ListView_GetNextItem(portfolio_page.positions, -1, LVNI_SELECTED);
    if (selected < 0) {
        SetWindowTextW(portfolio_page.position_stop, L""); SetWindowTextW(portfolio_page.position_take, L"");
        return;
    }
    wchar_t ticker_text[32]{};
    ListView_GetItemText(portfolio_page.positions, selected, 0, ticker_text, static_cast<int>(std::size(ticker_text)));
    const auto ticker = narrow(ticker_text);
    const auto position = open_position_for_ticker(ticker);
    if (!position) return;
    const auto* levels = levels_for_position(position->ticker, position->opened_on);
    SetWindowTextW(portfolio_page.position_stop, levels && levels->stop_loss ? widen(money(*levels->stop_loss)).c_str() : L"");
    SetWindowTextW(portfolio_page.position_take, levels && levels->take_profit ? widen(money(*levels->take_profit)).c_str() : L"");

    select_instrument(ticker,true);

}

void Application::save_selected_position_levels() {
    const int selected = ListView_GetNextItem(portfolio_page.positions, -1, LVNI_SELECTED);
    if (selected < 0) {
        MessageBoxW(shell.window, L"Сначала выберите открытую позицию в таблице.", L"Уровни позиции", MB_ICONINFORMATION);
        return;
    }
    wchar_t ticker_text[32]{};
    ListView_GetItemText(portfolio_page.positions, selected, 0, ticker_text, static_cast<int>(std::size(ticker_text)));
    const auto ticker = narrow(ticker_text);
    const auto position = open_position_for_ticker(ticker);
    if (!position) {
        MessageBoxW(shell.window, L"Выбранная позиция уже закрыта. Обновите портфель.", L"Уровни позиции", MB_ICONWARNING);
        return;
    }
    const auto stop_text = narrow(read_edit(portfolio_page.position_stop));
    const auto take_text = narrow(read_edit(portfolio_page.position_take));
    const auto stop = stop_text.empty() ? std::optional<double>{} : parse_finite_number(stop_text);
    const auto take = take_text.empty() ? std::optional<double>{} : parse_finite_number(take_text);
    PositionLevels levels{position->ticker, position->opened_on, stop, take};
    if ((!stop_text.empty() && !stop) || (!take_text.empty() && !take) || !valid_position_levels(levels)) {
        MessageBoxW(shell.window, L"Введите положительные цены для стоп-лосса и тейк-профита. Пустое поле отключает уровень.",
                    L"Некорректный уровень", MB_ICONWARNING);
        return;
    }
    std::erase_if(portfolio_page.position_levels, [&](const auto& existing) {
        return existing.ticker == levels.ticker && existing.opened_on == levels.opened_on;
    });
    if (levels.stop_loss || levels.take_profit) portfolio_page.position_levels.push_back(std::move(levels));
    save_position_levels(); fill_portfolio(); InvalidateRect(shell.window, nullptr, TRUE);
}

void Application::add_portfolio_transaction() {
    const auto ticker_index = SendMessageW(portfolio_page.ticker, CB_GETCURSEL, 0, 0);
    const auto side_index = SendMessageW(portfolio_page.side, CB_GETCURSEL, 0, 0);
    if (ticker_index < 0 || static_cast<std::size_t>(ticker_index) >= state.market.instruments.size() || (side_index != 0 && side_index != 1)) {
        MessageBoxW(shell.window, L"Добавьте тикер в список наблюдения и выберите покупку или продажу.", L"Портфель", MB_ICONWARNING);
        return;
    }
    const auto quantity = parse_finite_number(narrow(read_edit(portfolio_page.quantity)));
    const auto price = parse_finite_number(narrow(read_edit(portfolio_page.price)));
    const auto commission_text = narrow(read_edit(portfolio_page.commission));
    const auto commission = commission_text.empty() ? std::optional<double>(0.0) : parse_finite_number(commission_text);
    const auto ticker = state.market.instruments[static_cast<std::size_t>(ticker_index)].ticker;
    const PortfolioTransaction transaction{narrow(read_edit(portfolio_page.date)), ticker,
        side_index == 0 ? PortfolioSide::Buy : PortfolioSide::Sell,
        quantity.value_or(0.0), price.value_or(0.0), commission.value_or(-1.0)};
    if (!quantity || !price || !commission || !valid_portfolio_transaction(transaction)) {
        MessageBoxW(shell.window, L"Проверьте дату ГГГГ-ММ-ДД, количество, цену и комиссию. Количество и цена должны быть больше нуля.",
                    L"Некорректная операция", MB_ICONWARNING);
        return;
    }
    auto candidate = portfolio_page.transactions;
    candidate.push_back(transaction);
    std::stable_sort(candidate.begin(), candidate.end(), [](const auto& a, const auto& b) { return a.date < b.date; });
    if (!calculate_portfolio(candidate, {})) {
        MessageBoxW(shell.window, L"Продажа превышает доступный остаток акций или в журнале уже есть несогласованные операции.",
                    L"Недостаточно акций", MB_ICONWARNING);
        return;
    }
    if (transaction.side == PortfolioSide::Sell) {
        const auto previous_position = open_position_for_ticker(ticker);
        if (previous_position && transaction.quantity >= previous_position->quantity - 1e-9) {
            std::erase_if(portfolio_page.position_levels, [&](const auto& levels) {
                return levels.ticker == previous_position->ticker && levels.opened_on == previous_position->opened_on;
            });
            save_position_levels();
        }
    }
    portfolio_page.transactions = std::move(candidate);
    save_portfolio(); fill_portfolio();
    SetWindowTextW(portfolio_page.quantity, L""); SetWindowTextW(portfolio_page.price, L""); SetWindowTextW(portfolio_page.commission, L"");
    SetWindowTextW(portfolio_page.date, widen(today_date()).c_str());
}

void Application::delete_portfolio_transaction() {
    const auto selected_id=selected_row_data(portfolio_page.transactions_list);
    const int selected=selected_id?static_cast<int>(*selected_id):-1;
    if (selected < 0 || static_cast<std::size_t>(selected) >= portfolio_page.transactions.size()) return;
    portfolio_page.transactions.erase(portfolio_page.transactions.begin() + selected);
    save_portfolio(); fill_portfolio();
}

std::optional<std::filesystem::path> Application::select_operations_csv(bool save) {
    std::vector<wchar_t> buffer(32768,L'\0');
    if(save) { const std::wstring name=L"MOEXWatch-operations.csv"; std::copy(name.begin(),name.end(),buffer.begin()); }
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=shell.window;
    dialog.lpstrFilter=L"CSV UTF-8 (*.csv)\0*.csv\0Все файлы (*.*)\0*.*\0";
    dialog.lpstrFile=buffer.data();dialog.nMaxFile=static_cast<DWORD>(buffer.size());dialog.lpstrDefExt=L"csv";
    dialog.lpstrTitle=save?L"Экспорт операций портфеля":L"Выберите CSV операций для просмотра";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    const BOOL ok=save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog);
    if(!ok){if(CommDlgExtendedError()!=0)throw std::runtime_error("Не удалось открыть диалог выбора файла");return {};}
    return std::filesystem::path(buffer.data());
}

void Application::export_portfolio_operations() {
    try {
        if(state.persistence.writes_blocked)throw std::runtime_error("Сначала устраните ошибку загрузки журналов, чтобы не экспортировать неполный портфель");
        const auto path=select_operations_csv(true);if(!path)return;
        auto target=std::filesystem::weakly_canonical(*path).wstring();
        auto root=std::filesystem::weakly_canonical(storage::data_directory()).wstring();
        const auto lowercase=[](std::wstring& value){for(auto& c:value)c=static_cast<wchar_t>(std::towlower(c));};lowercase(target);lowercase(root);
        if(target==root || target.starts_with(root+L"\\"))throw std::runtime_error("Выберите папку вне APPDATA/MOEXWatch: экспорт не должен заменять рабочие данные или резервные копии");
        storage::atomic_write(*path,export_portfolio_csv(portfolio_page.transactions));
        MessageBoxW(shell.window,(L"Операции экспортированы:\n"+path->wstring()+L"\nФормат: CSV UTF-8, разделитель ;, количество указано в акциях.").c_str(),L"Экспорт CSV",MB_ICONINFORMATION);
    }catch(const std::exception& ex){MessageBoxW(shell.window,widen(ex.what()).c_str(),L"Экспорт не выполнен",MB_ICONERROR);}
}

void Application::import_portfolio_operations() {
    try {
        if(state.persistence.writes_blocked)throw std::runtime_error("Сначала устраните ошибку хранения пользовательских данных");
        const auto path=select_operations_csv(false);if(!path)return;
        const auto csv=storage::read_text(*path);if(!csv)throw std::runtime_error("CSV-файл не найден");
        // Synchronize existing state without changing its content before opening a preview.
        save_watchlist();save_trades();save_portfolio();save_position_levels();save_signal_settings();
        if(state.persistence.writes_blocked)throw std::runtime_error("Не удалось сохранить текущие записи; импорт отменён");
        const auto root=storage::data_directory();const auto original=storage::read_text(root/"portfolio_transactions.csv");
        std::filesystem::path backup;
        const bool imported=review_portfolio_import(shell.window,shell.font,*csv,portfolio_page.transactions,[&](const auto& candidate){
            if(storage::read_text(root/"portfolio_transactions.csv")!=original)throw std::runtime_error("Журнал изменился после открытия просмотра. Закройте окно и повторите импорт.");
            backup=storage::create_backup(root);
            if(storage::read_text(root/"portfolio_transactions.csv")!=original)throw std::runtime_error("Журнал изменился во время резервирования. Повторите импорт.");
            storage::save_portfolio(root,candidate);
            portfolio_page.transactions=candidate;
        });
        if(imported){fill_portfolio();MessageBoxW(shell.window,(L"Операции добавлены. Исходные данные сохранены:\n"+backup.wstring()+L"\nДля котировок новых тикеров добавьте их в список наблюдения.").c_str(),L"Импорт завершён",MB_ICONINFORMATION);}
    }catch(const std::exception& ex){MessageBoxW(shell.window,widen(ex.what()).c_str(),L"Импорт не выполнен",MB_ICONERROR);}
}

void Application::create_portfolio_controls(HWND window) {
    add_control(portfolio_page.controls,window, L"STATIC", L"Дата", 0, 16, 59, 42, 24, 0);
    add_control(portfolio_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 60, 55, 105, 28, ID_PORT_DATE);
    portfolio_page.date = GetDlgItem(window, ID_PORT_DATE);
    add_control(portfolio_page.controls,window, L"STATIC", L"Тикер", 0, 178, 59, 48, 24, 0);
    portfolio_page.ticker = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        229, 55, 115, 220, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_TICKER)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(portfolio_page.ticker, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    portfolio_page.controls.push_back(portfolio_page.ticker);
    add_control(portfolio_page.controls,window, L"STATIC", L"Операция", 0, 356, 59, 68, 24, 0);
    portfolio_page.side = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        428, 55, 110, 100, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_SIDE)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(portfolio_page.side, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    portfolio_page.controls.push_back(portfolio_page.side);
    SendMessageW(portfolio_page.side, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Покупка"));
    SendMessageW(portfolio_page.side, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Продажа"));
    SendMessageW(portfolio_page.side, CB_SETCURSEL, 0, 0);
    add_control(portfolio_page.controls,window, L"STATIC", L"Количество", 0, 550, 59, 72, 24, 0);
    add_control(portfolio_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 626, 55, 95, 28, ID_PORT_QUANTITY);
    portfolio_page.quantity = GetDlgItem(window, ID_PORT_QUANTITY);
    add_control(portfolio_page.controls,window, L"STATIC", L"Цена, ₽", 0, 16, 99, 58, 24, 0);
    add_control(portfolio_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 78, 95, 105, 28, ID_PORT_PRICE);
    portfolio_page.price = GetDlgItem(window, ID_PORT_PRICE);
    add_control(portfolio_page.controls,window, L"STATIC", L"Комиссия, ₽", 0, 203, 99, 100, 24, 0);
    add_control(portfolio_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 308, 95, 105, 28, ID_PORT_COMMISSION);
    portfolio_page.commission = GetDlgItem(window, ID_PORT_COMMISSION);
    portfolio_page.add = CreateWindowExW(0, L"BUTTON", L"Записать операцию", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        435, 95, 155, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_ADD)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(portfolio_page.add, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    portfolio_page.controls.push_back(portfolio_page.add);
    add_control(portfolio_page.controls,window,L"BUTTON",L"Импорт CSV",BS_PUSHBUTTON,620,95,125,28,ID_PORT_IMPORT);
    add_control(portfolio_page.controls,window,L"BUTTON",L"Экспорт CSV",BS_PUSHBUTTON,758,95,125,28,ID_PORT_EXPORT);
    add_control(portfolio_page.controls,window, L"STATIC", L"Тикер выбирается из списка наблюдения. В себестоимость покупки включается комиссия; продажа списывается по средней цене.",
        0, 16, 132, 1220, 22, 0);
    portfolio_page.summary = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 157, 1240, 24, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_SUMMARY)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(portfolio_page.summary, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    portfolio_page.controls.push_back(portfolio_page.summary);
    portfolio_page.positions = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        16, 185, 1260, 220, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_POSITIONS)), GetModuleHandleW(nullptr), nullptr);
    portfolio_page.controls.push_back(portfolio_page.positions);
    ListView_SetExtendedListViewStyle(portfolio_page.positions, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* position_headers[] = {L"Тикер",L"Акций",L"Средняя, ₽",L"Котировка, ₽",L"Стоимость, ₽",L"Доля, %",L"Бумажная прибыль, ₽",L"Бумажная, %",L"Зафиксировано, ₽",L"Стоп-лосс, ₽",L"Тейк-профит, ₽"};
    const int position_widths[] = {75,80,105,110,115,90,155,100,145,110,125};
    for (int i=0;i<11;++i) { LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.pszText=const_cast<wchar_t*>(position_headers[i]); col.cx=position_widths[i]; ListView_InsertColumn(portfolio_page.positions,i,&col); }
    add_control(portfolio_page.controls,window, L"STATIC", L"Стоп-лосс, ₽", 0, 16, 418, 105, 24, 0);
    add_control(portfolio_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 121, 414, 110, 28, ID_POSITION_STOP);
    portfolio_page.position_stop = GetDlgItem(window, ID_POSITION_STOP);
    add_control(portfolio_page.controls,window, L"STATIC", L"Тейк-профит, ₽", 0, 248, 418, 115, 24, 0);
    add_control(portfolio_page.controls,window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 367, 414, 110, 28, ID_POSITION_TAKE);
    portfolio_page.position_take = GetDlgItem(window, ID_POSITION_TAKE);
    portfolio_page.position_levels_save = CreateWindowExW(0, L"BUTTON", L"Сохранить уровни", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        494, 414, 155, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_POSITION_LEVELS_SAVE)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(portfolio_page.position_levels_save, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    portfolio_page.controls.push_back(portfolio_page.position_levels_save);
    add_control(portfolio_page.controls,window, L"STATIC", L"Выберите позицию; пустое поле отключает уровень. Линии также показываются на графике выбранной акции.",
        0, 664, 418, 610, 24, 0);
    add_control(portfolio_page.controls,window, L"STATIC", L"Журнал операций", 0, 16, 466, 160, 24, 0);
    portfolio_page.delete_button = CreateWindowExW(0, L"BUTTON", L"Удалить выбранную", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        1060, 458, 170, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_DELETE)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(portfolio_page.delete_button, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    portfolio_page.controls.push_back(portfolio_page.delete_button);
    portfolio_page.transactions_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        16, 492, 1260, 300, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_TRANSACTIONS)), GetModuleHandleW(nullptr), nullptr);
    portfolio_page.controls.push_back(portfolio_page.transactions_list);
    ListView_SetExtendedListViewStyle(portfolio_page.transactions_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* operation_headers[] = {L"Дата",L"Тикер",L"Операция",L"Акций",L"Цена, ₽",L"Комиссия, ₽"};
    const int operation_widths[] = {110,95,120,100,120,130};
    for (int i=0;i<6;++i) { LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.pszText=const_cast<wchar_t*>(operation_headers[i]); col.cx=operation_widths[i]; ListView_InsertColumn(portfolio_page.transactions_list,i,&col); }
    if (shell.font) SendMessageW(portfolio_page.transactions_list, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);

}

void Application::resize_portfolio(int width, int height) {
    (void)height;
        if (portfolio_page.summary) MoveWindow(portfolio_page.summary, 16, 157, width - 32, 24, TRUE);
        if (portfolio_page.positions) MoveWindow(portfolio_page.positions, 16, 185, width - 32, 220, TRUE);
        if (portfolio_page.transactions_list) MoveWindow(portfolio_page.transactions_list, 16, 492, width - 32, std::max(100, height - 512), TRUE);
        if (portfolio_page.delete_button) MoveWindow(portfolio_page.delete_button, std::max(870, width - 190), 458, 170, 28, TRUE);
}

bool Application::command_portfolio(WPARAM wparam) {
    switch(LOWORD(wparam)) {
        case ID_PORT_ADD: add_portfolio_transaction(); return true;
        case ID_PORT_IMPORT: import_portfolio_operations(); return true;
        case ID_PORT_EXPORT: export_portfolio_operations(); return true;
        case ID_PORT_DELETE: delete_portfolio_transaction(); return true;
        case ID_POSITION_LEVELS_SAVE: save_selected_position_levels(); return true;
    }
    return false;
}
}
#endif
