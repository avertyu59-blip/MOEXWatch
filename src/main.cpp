#include "market_core.hpp"
#include "app_models.hpp"
#include "storage.hpp"
#include "market_data.hpp"
#include "portfolio_csv.hpp"
#include "portfolio_import_ui.hpp"
#include "observation.hpp"
#include "observation_ui.hpp"
#include "table_sort_win.hpp"
#include <map>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <cwctype>
#include <shellapi.h>

using namespace moexwatch;

namespace {
constexpr UINT WM_DATA_READY = WM_APP + 10;
constexpr UINT WM_FETCH_PROGRESS = WM_APP + 11;
constexpr UINT WM_BACKTEST_READY = WM_APP + 12;
constexpr UINT WM_DIVIDENDS_READY = WM_APP + 13;
constexpr UINT WM_BACKUP_READY = WM_APP + 14;
constexpr int ID_BACKUP = 1060, ID_PORT_IMPORT = 1061, ID_PORT_EXPORT = 1062;
constexpr int ID_TICKER = 1001, ID_TARGET = 1002, ID_ADD = 1003, ID_TARGET_SET = 1004,
              ID_REMOVE = 1005, ID_REFRESH = 1006, ID_LIST = 1007, ID_FILTER = 1008, ID_RANGE = 1009;
constexpr int ID_TABS = 1010, ID_TRADE_TICKER = 1011, ID_TRADE_QUANTITY = 1012,
              ID_TRADE_BUY = 1013, ID_TRADE_SELL = 1014, ID_TRADE_COMMISSION = 1015,
              ID_TRADE_TAX = 1016, ID_TRADE_ADD = 1017, ID_TRADE_DELETE = 1018, ID_TRADE_LIST = 1019,
              ID_TRADE_SUMMARY = 1020, ID_SIG_OVERSOLD = 1021, ID_SIG_OVERSOLD_VALUE = 1022,
              ID_SIG_OVERBOUGHT = 1023, ID_SIG_OVERBOUGHT_VALUE = 1024, ID_SIG_SMA = 1025,
              ID_SIG_TARGET = 1026, ID_SIG_TARGET_VALUE = 1027, ID_SIG_MODE = 1028,
              ID_SIG_NOTIFY = 1029, ID_SIG_SAVE = 1030, ID_SIG_LIST = 1031, ID_SIG_SUMMARY = 1032,
              ID_SIG_AUTO_REFRESH = 1033, ID_SIG_INTERVAL = 1034,
              ID_BACKTEST_TICKER = 1035, ID_BACKTEST_CAPITAL = 1036, ID_BACKTEST_FEE = 1037,
              ID_BACKTEST_RUN = 1038, ID_BACKTEST_RESULT = 1039,
              ID_PORT_DATE = 1040, ID_PORT_TICKER = 1041, ID_PORT_SIDE = 1042,
              ID_PORT_QUANTITY = 1043, ID_PORT_PRICE = 1044, ID_PORT_COMMISSION = 1045,
              ID_PORT_ADD = 1046, ID_PORT_DELETE = 1047, ID_PORT_POSITIONS = 1048,
              ID_PORT_TRANSACTIONS = 1049, ID_PORT_SUMMARY = 1050,
              ID_POSITION_STOP = 1051, ID_POSITION_TAKE = 1052, ID_POSITION_LEVELS_SAVE = 1053;
constexpr int ID_DIVIDENDS_OPEN = 1054, ID_DIVIDENDS_LIST = 1055, ID_DIVIDENDS_REFRESH = 1056;
constexpr int ID_CHART_TABS = 1057, ID_DIV_CHART_TABS = 1058, ID_DIV_CHART_RANGE = 1059;
constexpr UINT_PTR TIMER_AUTO_REFRESH = 1;


HWND g_window{};
HWND g_observation_page{};
HWND g_backup_button{};
HWND g_list{};
HWND g_ticker_edit{};
HWND g_target_edit{};
HWND g_status{};
HWND g_analysis{};
HWND g_tabs{};
HWND g_dividend_window{};
HWND g_dividend_list{};
HWND g_dividend_info{};
HWND g_dividend_refresh{};
HWND g_chart_tabs{};
HWND g_dividend_chart_tabs{};
HWND g_dividend_chart_range{};
HWND g_trade_list{};
HWND g_trade_summary{};
HWND g_trade_ticker{};
HWND g_trade_quantity{};
HWND g_trade_buy{};
HWND g_trade_sell{};
HWND g_trade_commission{};
HWND g_trade_tax{};
HWND g_signal_list{};
HWND g_signal_summary{};
HWND g_signal_oversold_value{};
HWND g_signal_overbought_value{};
HWND g_signal_target_value{};
HWND g_signal_interval{};
HWND g_backtest_ticker{};
HWND g_backtest_capital{};
HWND g_backtest_fee{};
HWND g_backtest_run{};
HWND g_backtest_result{};
HWND g_port_date{};
HWND g_port_ticker{};
HWND g_port_side{};
HWND g_port_quantity{};
HWND g_port_price{};
HWND g_port_commission{};
HWND g_port_add{};
HWND g_port_delete{};
HWND g_port_positions{};
HWND g_port_transactions_list{};
HWND g_port_summary{};
HWND g_position_stop{};
HWND g_position_take{};
HWND g_position_levels_save{};
HFONT g_font{};
std::vector<Instrument> g_items;
std::vector<Candle> g_imoex_candles;
bool g_imoex_stale{true};
std::string g_imoex_error;
std::atomic<bool> g_busy{false};
std::size_t g_selected{};
std::size_t g_dividend_instrument{};
unsigned int g_dividend_request{};
std::vector<std::size_t> g_visible;
bool g_filling = false;
bool g_building_trade_page = false;
bool g_building_signal_page = false;
bool g_building_backtest_page = false;
bool g_building_portfolio_page = false;
std::atomic<bool> g_backtest_busy{false};
int g_active_page = 0;
std::vector<HWND> g_market_controls;
std::vector<HWND> g_trade_controls;
std::vector<HWND> g_signal_controls;
std::vector<HWND> g_backtest_controls;
std::vector<HWND> g_portfolio_controls;
std::vector<HWND> g_global_controls;
std::vector<Trade> g_trades;
std::vector<PortfolioTransaction> g_portfolio_transactions;
std::vector<PositionLevels> g_position_levels;
SignalSettings g_signal_settings;
ObservationEngine g_observer;
std::vector<SignalEvent> g_signal_history;
std::string g_observation_error;
std::string g_observation_status="Первый свежий ответ задаёт исходное состояние без уведомления.";
bool g_observation_writable=true;
bool g_history_dirty=false;
std::map<int,TableSort> g_table_sorts;
void apply_table_sort(HWND list,int id) {
    const auto found=g_table_sorts.find(id);if(found==g_table_sorts.end())return;
    const int col=found->second.column;
    bool numeric=false;
    if(id==ID_LIST)numeric=col>=1&&col<=4;
    else if(id==ID_TRADE_LIST)numeric=col>=1;
    else if(id==ID_PORT_POSITIONS)numeric=col>=1;
    else if(id==ID_PORT_TRANSACTIONS)numeric=col>=3;
    else if(id==ID_SIG_LIST)numeric=col>=1&&col<=4;
    else if(id==ID_DIVIDENDS_LIST)numeric=col==2||col==3;
    const bool old=g_filling;g_filling=true;sort_report(list,found->second,numeric);g_filling=old;
}
bool g_signal_baseline_ready = false;
NOTIFYICONDATAW g_notification_icon{};
bool g_notification_icon_ready = false;
WatchFilter g_filter = WatchFilter::All;
int g_chart_sessions = 66;
int selected_instrument() {
    const auto selected=selected_row_data(g_list);
    return selected && *selected<g_items.size()?static_cast<int>(*selected):-1;
}

std::wstring widen(std::string_view text) {
    if (text.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}

std::string narrow(std::wstring_view text) {
    if (text.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring read_edit(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

bool g_storage_blocked{};
void storage_failure(const char* error) {
    g_storage_blocked = true;
    MessageBoxW(g_window, widen(std::string("Ошибка хранения данных: ") + error +
        "\nСделайте резервную копию. До исправления ошибки автоматическая запись отключена.").c_str(), L"MOEXWatch", MB_ICONERROR);
}

std::optional<double> parse_finite_number(std::string value);

void save_signal_settings() {
    if (g_storage_blocked) return;
    try { storage::save_signal_settings(storage::data_directory(), g_signal_settings); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void load_signal_settings() {
    try { auto loaded = g_signal_settings; storage::load_signal_settings(storage::data_directory(), loaded); g_signal_settings = std::move(loaded); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

std::optional<double> parse_finite_number(std::string value) {
    std::replace(value.begin(), value.end(), ',', '.');
    if (value.empty()) return std::nullopt;
    double parsed{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() || !std::isfinite(parsed)) return std::nullopt;
    return parsed;
}

void update_auto_refresh_timer() {
    if (!g_window) return;
    KillTimer(g_window, TIMER_AUTO_REFRESH);
    if (g_signal_settings.auto_refresh_enabled) {
        const auto milliseconds = static_cast<UINT>(g_signal_settings.auto_refresh_minutes) * 60U * 1000U;
        SetTimer(g_window, TIMER_AUTO_REFRESH, milliseconds, nullptr);
    }
}

std::string money(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

void save_trades() {
    if (g_storage_blocked) return;
    try { storage::save_trades(storage::data_directory(), g_trades); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void load_trades() {
    try { auto loaded = g_trades; storage::load_trades(storage::data_directory(), loaded); g_trades = std::move(loaded); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void save_portfolio() {
    if (g_storage_blocked) return;
    try { storage::save_portfolio(storage::data_directory(), g_portfolio_transactions); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void load_portfolio() {
    try { auto loaded = g_portfolio_transactions; storage::load_portfolio(storage::data_directory(), loaded); g_portfolio_transactions = std::move(loaded); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void save_position_levels() {
    if (g_storage_blocked) return;
    try { storage::save_position_levels(storage::data_directory(), g_position_levels); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void load_position_levels() {
    try { auto loaded = g_position_levels; storage::load_position_levels(storage::data_directory(), loaded); g_position_levels = std::move(loaded); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

std::string optional_number(std::optional<double> n) {
    return n ? format_price(*n) : "—";
}

std::string percent(std::optional<double> n) {
    if (!n) return "—";
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << *n << '%';
    return out.str();
}

void save_watchlist() {
    if (g_storage_blocked) return;
    try { storage::save_watchlist(storage::data_directory(), g_items); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}

void load_watchlist() {
    try { auto loaded = g_items; storage::load_watchlist(storage::data_directory(), loaded); g_items = std::move(loaded); }
    catch (const std::exception& ex) { storage_failure(ex.what()); }
}


std::string today_date();









void refresh_data() {
    bool expected = false;
    if (!g_busy.compare_exchange_strong(expected, true)) return;
    EnableWindow(GetDlgItem(g_window, ID_REFRESH), FALSE);
    SetWindowTextW(g_status, L"Загружаю акции и историю IMOEX из MOEX ISS…");
    const auto source = g_items;
    const HWND target = g_window;
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

std::string status_for(const Instrument& item) {
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

void update_analysis();

void fill_list() {
    g_filling = true;
    ListView_DeleteAllItems(g_list);
    g_visible.clear();
    for (std::size_t i = 0; i < g_items.size(); ++i) {
        const auto& item = g_items[i];
        const auto analysis = analyze(item.candles);
        const bool stale = item.quote_stale ||
            (g_filter != WatchFilter::NearTarget && item.candles_stale);
        if (!matches_filter(analysis, item.quote.last, item.target, stale, g_filter)) continue;
        const int row_index = static_cast<int>(g_visible.size());
        g_visible.push_back(i);
        LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(i); row.iItem = row_index;
        auto ticker = widen(item.ticker); row.pszText = ticker.data();
        ListView_InsertItem(g_list, &row);
        const std::string values[] = {optional_number(item.quote.last), percent(item.quote.change_percent),
            optional_number(item.target), optional_number(analysis.rsi14), status_for(item)};
        for (int col = 1; col <= 5; ++col) {
            auto wide = widen(values[col - 1]); ListView_SetItemText(g_list, row_index, col, wide.data());
        }
    }
    if (!g_visible.empty()) {
        const auto selected = std::find(g_visible.begin(), g_visible.end(), g_selected);
        const auto row = selected == g_visible.end() ? 0 : static_cast<int>(selected - g_visible.begin());
        g_selected = g_visible[static_cast<std::size_t>(row)];
        ListView_SetItemState(g_list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    apply_table_sort(g_list,ID_LIST);
    g_filling = false;
    update_analysis();
}

void update_analysis() {
    if (!g_analysis) return;
    if (g_items.empty() || g_visible.empty()) {
        SetWindowTextW(g_analysis, L"Нет акций, соответствующих фильтру. Выберите «Все акции». ");
        InvalidateRect(g_window, nullptr, TRUE); return;
    }
    g_selected = std::min(g_selected, g_items.size() - 1);
    const auto& item = g_items[g_selected];
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
    if (!g_imoex_error.empty()) text << "\r\nИстория IMOEX: " << g_imoex_error;
    text << "\r\nТехнические индикаторы описывают прошлые цены и не являются сигналом к покупке.";
    SetWindowTextW(g_analysis, widen(text.str()).c_str());
    InvalidateRect(g_window, nullptr, TRUE);
}

void fill_trades() {
    if (!g_trade_list) return;
    ListView_DeleteAllItems(g_trade_list);
    TradeResult totals{};
    for (std::size_t i = 0; i < g_trades.size(); ++i) {
        const auto& trade = g_trades[i];
        const auto result = calculate_trade(trade);
        if (!result) continue;
        totals.gross_profit += result->gross_profit;
        totals.profit_after_commission += result->profit_after_commission;
        totals.tax += result->tax;
        totals.net_profit += result->net_profit;
        LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(i); row.iItem = static_cast<int>(i);
        auto ticker = widen(trade.ticker); row.pszText = ticker.data();
        ListView_InsertItem(g_trade_list, &row);
        const std::string values[] = {money(trade.quantity), money(trade.buy_price), money(trade.sell_price),
            money(result->gross_profit), money(trade.commission), percent(trade.tax_rate_percent),
            money(result->tax), money(result->net_profit)};
        for (int col = 1; col <= 8; ++col) {
            auto wide = widen(values[col - 1]); ListView_SetItemText(g_trade_list, static_cast<int>(i), col, wide.data());
        }
    }
    const auto summary = L"Сделок: " + std::to_wstring(g_trades.size()) +
        L"     Валовая прибыль: " + widen(money(totals.gross_profit)) + L" ₽" +
        L"     После комиссий: " + widen(money(totals.profit_after_commission)) + L" ₽" +
        L"     Налог (оценка): " + widen(money(totals.tax)) + L" ₽" +
        L"     Чистая прибыль: " + widen(money(totals.net_profit)) + L" ₽";
    SetWindowTextW(g_trade_summary, summary.c_str());
    apply_table_sort(g_trade_list,ID_TRADE_LIST);
}

std::vector<Quote> portfolio_quotes() {
    std::vector<Quote> quotes;
    quotes.reserve(g_items.size());
    for (const auto& item : g_items) quotes.push_back(item.quote);
    return quotes;
}

const PositionLevels* levels_for_position(std::string_view ticker, std::string_view opened_on) {
    const auto found = std::find_if(g_position_levels.begin(), g_position_levels.end(), [&](const auto& levels) {
        return levels.ticker == ticker && levels.opened_on == opened_on;
    });
    return found == g_position_levels.end() ? nullptr : &*found;
}

std::optional<PortfolioPosition> open_position_for_ticker(std::string_view ticker) {
    const auto portfolio = calculate_portfolio(g_portfolio_transactions, {});
    if (!portfolio) return std::nullopt;
    const auto found = std::find_if(portfolio->positions.begin(), portfolio->positions.end(), [&](const auto& position) {
        return position.ticker == ticker && position.quantity > 0.0;
    });
    return found == portfolio->positions.end() ? std::nullopt : std::optional<PortfolioPosition>(*found);
}

std::string portfolio_side_name(PortfolioSide side) {
    return side == PortfolioSide::Buy ? "Покупка" : "Продажа";
}

void fill_portfolio() {
    if (!g_port_positions || !g_port_transactions_list || !g_port_summary) return;
    ListView_DeleteAllItems(g_port_positions);
    ListView_DeleteAllItems(g_port_transactions_list);
    const auto portfolio = calculate_portfolio(g_portfolio_transactions, portfolio_quotes());
    if (portfolio) {
        const auto old_size = g_position_levels.size();
        std::erase_if(g_position_levels, [&](const auto& levels) {
            return std::none_of(portfolio->positions.begin(), portfolio->positions.end(), [&](const auto& position) {
                return position.quantity > 0.0 && position.ticker == levels.ticker && position.opened_on == levels.opened_on;
            });
        });
        if (g_position_levels.size() != old_size) save_position_levels();
    }
    const bool allocation_fresh = portfolio && portfolio->priced_positions == portfolio->open_positions &&
        std::all_of(portfolio->positions.begin(),portfolio->positions.end(),[](const auto& position) {
            if(position.quantity<=0.0)return true;
            const auto source=std::find_if(g_items.begin(),g_items.end(),[&](const auto& item){return item.ticker==position.ticker;});
            return source!=g_items.end() && !source->quote_stale;
        });
    std::size_t row_index = 0;
    if (portfolio) {
        for (const auto& position : portfolio->positions) {
            if (position.quantity <= 0.0) continue;
            LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(row_index); row.iItem = static_cast<int>(row_index);
            auto ticker = widen(position.ticker); row.pszText = ticker.data();
            ListView_InsertItem(g_port_positions, &row);
            bool stale = false;
            const auto source = std::find_if(g_items.begin(), g_items.end(), [&](const auto& item) { return item.ticker == position.ticker; });
            if (source != g_items.end()) stale = source->quote_stale;
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
                auto wide = widen(values[col - 1]); ListView_SetItemText(g_port_positions, static_cast<int>(row_index), col, wide.data());
            }
            ++row_index;
        }
    }
    for (std::size_t i = 0; i < g_portfolio_transactions.size(); ++i) {
        const auto& transaction = g_portfolio_transactions[i];
        LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(i); row.iItem = static_cast<int>(i);
        auto date = widen(transaction.date); row.pszText = date.data();
        ListView_InsertItem(g_port_transactions_list, &row);
        const std::string values[] = {transaction.ticker, portfolio_side_name(transaction.side),
            money(transaction.quantity), money(transaction.price), money(transaction.commission)};
        for (int col = 1; col <= 5; ++col) {
            auto wide = widen(values[col - 1]); ListView_SetItemText(g_port_transactions_list, static_cast<int>(i), col, wide.data());
        }
    }
    apply_table_sort(g_port_positions,ID_PORT_POSITIONS);
    apply_table_sort(g_port_transactions_list,ID_PORT_TRANSACTIONS);
    if (!portfolio) {
        SetWindowTextW(g_port_summary, L"В журнале есть продажа без достаточного остатка. Проверьте и удалите ошибочную операцию.");
        return;
    }
    const auto summary = L"Открытых позиций: " + std::to_wstring(portfolio->open_positions) +
        L"   Котировки: " + std::to_wstring(portfolio->priced_positions) + L"/" + std::to_wstring(portfolio->open_positions) +
        (allocation_fresh ? L"   Стоимость: " : L"   Стоимость оценённых позиций*: ") + widen(money(portfolio->total_market_value)) + L" ₽" +
        L"   Бумажная прибыль: " + widen(money(portfolio->total_unrealized_profit)) + L" ₽" +
        L"   Зафиксировано: " + widen(money(portfolio->total_realized_profit)) + (allocation_fresh ? L" ₽" : L" ₽ (* — неполная/устаревшая оценка; доли н/д)");
    SetWindowTextW(g_port_summary, summary.c_str());
}

void fill_position_level_editor() {
    if (!g_position_stop || !g_position_take || !g_port_positions) return;
    const int selected = ListView_GetNextItem(g_port_positions, -1, LVNI_SELECTED);
    if (selected < 0) {
        SetWindowTextW(g_position_stop, L""); SetWindowTextW(g_position_take, L"");
        return;
    }
    wchar_t ticker_text[32]{};
    ListView_GetItemText(g_port_positions, selected, 0, ticker_text, static_cast<int>(std::size(ticker_text)));
    const auto ticker = narrow(ticker_text);
    const auto position = open_position_for_ticker(ticker);
    if (!position) return;
    const auto* levels = levels_for_position(position->ticker, position->opened_on);
    SetWindowTextW(g_position_stop, levels && levels->stop_loss ? widen(money(*levels->stop_loss)).c_str() : L"");
    SetWindowTextW(g_position_take, levels && levels->take_profit ? widen(money(*levels->take_profit)).c_str() : L"");

    const auto instrument = std::find_if(g_items.begin(), g_items.end(), [&](const auto& item) { return item.ticker == ticker; });
    if (instrument != g_items.end()) {
        g_selected = static_cast<std::size_t>(instrument - g_items.begin());
        if (g_filter != WatchFilter::All) {
            g_filter = WatchFilter::All;
            SendMessageW(GetDlgItem(g_window, ID_FILTER), CB_SETCURSEL, 0, 0);
        }
        fill_list();
    }
}

void save_selected_position_levels() {
    const int selected = ListView_GetNextItem(g_port_positions, -1, LVNI_SELECTED);
    if (selected < 0) {
        MessageBoxW(g_window, L"Сначала выберите открытую позицию в таблице.", L"Уровни позиции", MB_ICONINFORMATION);
        return;
    }
    wchar_t ticker_text[32]{};
    ListView_GetItemText(g_port_positions, selected, 0, ticker_text, static_cast<int>(std::size(ticker_text)));
    const auto ticker = narrow(ticker_text);
    const auto position = open_position_for_ticker(ticker);
    if (!position) {
        MessageBoxW(g_window, L"Выбранная позиция уже закрыта. Обновите портфель.", L"Уровни позиции", MB_ICONWARNING);
        return;
    }
    const auto stop_text = narrow(read_edit(g_position_stop));
    const auto take_text = narrow(read_edit(g_position_take));
    const auto stop = stop_text.empty() ? std::optional<double>{} : parse_finite_number(stop_text);
    const auto take = take_text.empty() ? std::optional<double>{} : parse_finite_number(take_text);
    PositionLevels levels{position->ticker, position->opened_on, stop, take};
    if ((!stop_text.empty() && !stop) || (!take_text.empty() && !take) || !valid_position_levels(levels)) {
        MessageBoxW(g_window, L"Введите положительные цены для стоп-лосса и тейк-профита. Пустое поле отключает уровень.",
                    L"Некорректный уровень", MB_ICONWARNING);
        return;
    }
    std::erase_if(g_position_levels, [&](const auto& existing) {
        return existing.ticker == levels.ticker && existing.opened_on == levels.opened_on;
    });
    if (levels.stop_loss || levels.take_profit) g_position_levels.push_back(std::move(levels));
    save_position_levels(); fill_portfolio(); InvalidateRect(g_window, nullptr, TRUE);
}

std::string today_date() {
    SYSTEMTIME now{}; GetLocalTime(&now);
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << now.wYear << '-'
        << std::setw(2) << now.wMonth << '-' << std::setw(2) << now.wDay;
    return out.str();
}

void add_portfolio_transaction() {
    const auto ticker_index = SendMessageW(g_port_ticker, CB_GETCURSEL, 0, 0);
    const auto side_index = SendMessageW(g_port_side, CB_GETCURSEL, 0, 0);
    if (ticker_index < 0 || static_cast<std::size_t>(ticker_index) >= g_items.size() || (side_index != 0 && side_index != 1)) {
        MessageBoxW(g_window, L"Добавьте тикер в список наблюдения и выберите покупку или продажу.", L"Портфель", MB_ICONWARNING);
        return;
    }
    const auto quantity = parse_finite_number(narrow(read_edit(g_port_quantity)));
    const auto price = parse_finite_number(narrow(read_edit(g_port_price)));
    const auto commission_text = narrow(read_edit(g_port_commission));
    const auto commission = commission_text.empty() ? std::optional<double>(0.0) : parse_finite_number(commission_text);
    const auto ticker = g_items[static_cast<std::size_t>(ticker_index)].ticker;
    const PortfolioTransaction transaction{narrow(read_edit(g_port_date)), ticker,
        side_index == 0 ? PortfolioSide::Buy : PortfolioSide::Sell,
        quantity.value_or(0.0), price.value_or(0.0), commission.value_or(-1.0)};
    if (!quantity || !price || !commission || !valid_portfolio_transaction(transaction)) {
        MessageBoxW(g_window, L"Проверьте дату ГГГГ-ММ-ДД, количество, цену и комиссию. Количество и цена должны быть больше нуля.",
                    L"Некорректная операция", MB_ICONWARNING);
        return;
    }
    auto candidate = g_portfolio_transactions;
    candidate.push_back(transaction);
    std::stable_sort(candidate.begin(), candidate.end(), [](const auto& a, const auto& b) { return a.date < b.date; });
    if (!calculate_portfolio(candidate, {})) {
        MessageBoxW(g_window, L"Продажа превышает доступный остаток акций или в журнале уже есть несогласованные операции.",
                    L"Недостаточно акций", MB_ICONWARNING);
        return;
    }
    if (transaction.side == PortfolioSide::Sell) {
        const auto previous_position = open_position_for_ticker(ticker);
        if (previous_position && transaction.quantity >= previous_position->quantity - 1e-9) {
            std::erase_if(g_position_levels, [&](const auto& levels) {
                return levels.ticker == previous_position->ticker && levels.opened_on == previous_position->opened_on;
            });
            save_position_levels();
        }
    }
    g_portfolio_transactions = std::move(candidate);
    save_portfolio(); fill_portfolio();
    SetWindowTextW(g_port_quantity, L""); SetWindowTextW(g_port_price, L""); SetWindowTextW(g_port_commission, L"");
    SetWindowTextW(g_port_date, widen(today_date()).c_str());
}

void delete_portfolio_transaction() {
    const auto selected_id=selected_row_data(g_port_transactions_list);
    const int selected=selected_id?static_cast<int>(*selected_id):-1;
    if (selected < 0 || static_cast<std::size_t>(selected) >= g_portfolio_transactions.size()) return;
    g_portfolio_transactions.erase(g_portfolio_transactions.begin() + selected);
    save_portfolio(); fill_portfolio();
}

void add_trade() {
    std::string ticker = narrow(read_edit(g_trade_ticker));
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    const auto quantity = parse_finite_number(narrow(read_edit(g_trade_quantity)));
    const auto buy = parse_finite_number(narrow(read_edit(g_trade_buy)));
    const auto sell = parse_finite_number(narrow(read_edit(g_trade_sell)));
    const auto commission_text = narrow(read_edit(g_trade_commission));
    const auto tax_text = narrow(read_edit(g_trade_tax));
    const auto commission = commission_text.empty() ? std::optional<double>(0.0) : parse_finite_number(commission_text);
    const auto tax = tax_text.empty() ? std::optional<double>(0.0) : parse_finite_number(tax_text);
    if (!quantity || !buy || !sell || !commission || !tax) {
        MessageBoxW(g_window, L"Проверьте числа. Можно использовать точку или запятую; пустые комиссия и налог считаются равными 0.",
                    L"Не удалось добавить сделку", MB_ICONWARNING);
        return;
    }
    Trade trade{ticker, *quantity, *buy, *sell, *commission, *tax};
    if (!valid_trade(trade)) {
        MessageBoxW(g_window, L"Укажите корректный тикер, количество и цены больше нуля, комиссию от 0 ₽ и ставку налога от 0 до 100%.",
                    L"Не удалось добавить сделку", MB_ICONWARNING);
        return;
    }
    g_trades.push_back(std::move(trade));
    save_trades(); fill_trades();
    SetWindowTextW(g_trade_ticker, L""); SetWindowTextW(g_trade_quantity, L"");
    SetWindowTextW(g_trade_buy, L""); SetWindowTextW(g_trade_sell, L"");
    SetWindowTextW(g_trade_commission, L""); SetWindowTextW(g_trade_tax, L"");
    SetFocus(g_trade_ticker);
}

void delete_trade() {
    const auto selected_id=selected_row_data(g_trade_list);
    const int selected=selected_id?static_cast<int>(*selected_id):-1;
    if (selected < 0 || static_cast<std::size_t>(selected) >= g_trades.size()) return;
    g_trades.erase(g_trades.begin() + selected);
    save_trades(); fill_trades();
}

void synchronize_signal_controls() {
    CheckDlgButton(g_window, ID_SIG_OVERSOLD, g_signal_settings.rsi_oversold_enabled ? BST_CHECKED : BST_UNCHECKED);
    SetWindowTextW(g_signal_oversold_value, widen(money(g_signal_settings.rsi_oversold_below)).c_str());
    CheckDlgButton(g_window, ID_SIG_OVERBOUGHT, g_signal_settings.rsi_overbought_enabled ? BST_CHECKED : BST_UNCHECKED);
    SetWindowTextW(g_signal_overbought_value, widen(money(g_signal_settings.rsi_overbought_above)).c_str());
    CheckDlgButton(g_window, ID_SIG_SMA, g_signal_settings.above_sma200_enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(g_window, ID_SIG_TARGET, g_signal_settings.near_target_enabled ? BST_CHECKED : BST_UNCHECKED);
    SetWindowTextW(g_signal_target_value, widen(money(g_signal_settings.near_target_percent)).c_str());
    SendMessageW(GetDlgItem(g_window, ID_SIG_MODE), CB_SETCURSEL, g_signal_settings.require_all ? 1 : 0, 0);
    CheckDlgButton(g_window, ID_SIG_NOTIFY, g_signal_settings.notifications_enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(g_window, ID_SIG_AUTO_REFRESH, g_signal_settings.auto_refresh_enabled ? BST_CHECKED : BST_UNCHECKED);
    const int interval_index = g_signal_settings.auto_refresh_minutes == 30 ? 1 :
        (g_signal_settings.auto_refresh_minutes == 60 ? 2 : 0);
    SendMessageW(g_signal_interval, CB_SETCURSEL, interval_index, 0);
}

void fill_signals();

bool read_signal_settings_from_controls() {
    SignalSettings settings;
    settings.rsi_oversold_enabled = IsDlgButtonChecked(g_window, ID_SIG_OVERSOLD) == BST_CHECKED;
    settings.rsi_overbought_enabled = IsDlgButtonChecked(g_window, ID_SIG_OVERBOUGHT) == BST_CHECKED;
    settings.above_sma200_enabled = IsDlgButtonChecked(g_window, ID_SIG_SMA) == BST_CHECKED;
    settings.near_target_enabled = IsDlgButtonChecked(g_window, ID_SIG_TARGET) == BST_CHECKED;
    settings.notifications_enabled = IsDlgButtonChecked(g_window, ID_SIG_NOTIFY) == BST_CHECKED;
    settings.auto_refresh_enabled = IsDlgButtonChecked(g_window, ID_SIG_AUTO_REFRESH) == BST_CHECKED;
    const int interval_index = static_cast<int>(SendMessageW(g_signal_interval, CB_GETCURSEL, 0, 0));
    const int intervals[] = {15, 30, 60};
    if (interval_index < 0 || interval_index >= 3) {
        MessageBoxW(g_window, L"Выберите интервал автообновления.", L"Проверьте настройки", MB_ICONWARNING);
        return false;
    }
    settings.auto_refresh_minutes = intervals[interval_index];
    settings.require_all = SendMessageW(GetDlgItem(g_window, ID_SIG_MODE), CB_GETCURSEL, 0, 0) == 1;
    const auto oversold = parse_finite_number(narrow(read_edit(g_signal_oversold_value)));
    const auto overbought = parse_finite_number(narrow(read_edit(g_signal_overbought_value)));
    const auto near_target = parse_finite_number(narrow(read_edit(g_signal_target_value)));
    if (!oversold || !overbought || !near_target) {
        MessageBoxW(g_window, L"Введите числовые пороги RSI и расстояния до цели.", L"Проверьте условия", MB_ICONWARNING);
        return false;
    }
    settings.rsi_oversold_below = *oversold;
    settings.rsi_overbought_above = *overbought;
    settings.near_target_percent = *near_target;
    if (!valid_signal_settings(settings)) {
        MessageBoxW(g_window, L"Пороги RSI и расстояние до цели должны быть от 0 до 100.", L"Проверьте условия", MB_ICONWARNING);
        return false;
    }
    const bool changed=!same_technical_rules(g_signal_settings,settings);
    g_signal_settings = settings;
    save_signal_settings();
    update_auto_refresh_timer();
    if(changed)g_signal_baseline_ready = false;
    fill_signals();
    return true;
}

void fill_signals() {
    if (!g_signal_list || !g_signal_summary) return;
    ListView_DeleteAllItems(g_signal_list);
    std::size_t count = 0;
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for (const auto& item : g_items) {
        const auto analysis = analyze(item.candles);
        const auto evaluation = evaluate_signals(analysis, item.quote.last, item.target,
            item.quote_stale || !fresh_moex_quote(item.quote,now), item.candles_stale, g_signal_settings);
        if (!evaluation.matched) continue;
        ++count;
        LVITEMW row{}; row.mask = LVIF_TEXT|LVIF_PARAM; row.lParam=static_cast<LPARAM>(&item-g_items.data()); row.iItem = static_cast<int>(count - 1);
        auto ticker = widen(item.ticker); row.pszText = ticker.data();
        ListView_InsertItem(g_signal_list, &row);
        std::string reasons;
        for (std::size_t i = 0; i < evaluation.reasons.size(); ++i) {
            if (i) reasons += "; ";
            reasons += evaluation.reasons[i];
        }
        const std::string values[] = {optional_number(item.quote.last), optional_number(analysis.rsi14),
            optional_number(analysis.sma200), optional_number(item.target), reasons};
        for (int col = 1; col <= 5; ++col) {
            auto wide = widen(values[col - 1]); ListView_SetItemText(g_signal_list, static_cast<int>(count - 1), col, wide.data());
        }
    }
    const auto summary = L"Совпадений: " + std::to_wstring(count) +
        (g_signal_settings.require_all ? L"   Режим: все условия   " : L"   Режим: любое условие   ") +
        L"Котировка не старше 20 минут (МСК) и обновлённая история.";
    SetWindowTextW(g_signal_summary, summary.c_str());
    apply_table_sort(g_signal_list,ID_SIG_LIST);
}

void add_notification_icon() {
    g_notification_icon = {};
    g_notification_icon.cbSize = sizeof(g_notification_icon);
    g_notification_icon.hWnd = g_window;
    g_notification_icon.uID = 1;
    g_notification_icon.uFlags = NIF_ICON | NIF_TIP;
    g_notification_icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    const wchar_t tip[] = L"MOEX Watch — наблюдение за сигналами";
    std::copy_n(tip, std::min(std::size(tip), std::size(g_notification_icon.szTip)) - 1, g_notification_icon.szTip);
    g_notification_icon.szTip[std::size(g_notification_icon.szTip) - 1] = L'\0';
    g_notification_icon_ready = Shell_NotifyIconW(NIM_ADD, &g_notification_icon) != FALSE;
}

std::string signal_reason_text(const SignalEvaluation& evaluation) {
    std::string text;
    for (std::size_t i = 0; i < evaluation.reasons.size(); ++i) {
        if (i) text += "; ";
        text += evaluation.reasons[i];
    }
    return text;
}

void notify_new_signals() {
    if(!g_signal_baseline_ready){g_observer.reset_technical();g_signal_baseline_ready=true;}
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::vector<std::pair<std::string,std::string>> newly_matched;
    std::size_t fresh_count=0;
    for(const auto& item:g_items) {
        const auto evaluation=evaluate_signals(analyze(item.candles),item.quote.last,item.target,item.quote_stale,item.candles_stale,g_signal_settings);
        const bool fresh=!item.quote_stale && fresh_moex_quote(item.quote,now);if(fresh)++fresh_count;
        auto events=g_observer.observe(item.quote,fresh,!item.candles_stale,evaluation,g_signal_settings.require_all?"Все условия":"Любое условие",now);
        for(auto& event:events){newly_matched.emplace_back(event.ticker,event.condition+"; цена "+format_price(event.price)+"; котировка "+event.quote_time);g_signal_history.push_back(std::move(event));g_history_dirty=true;}
    }
    if(g_signal_history.size()>2000)g_signal_history.erase(g_signal_history.begin(),g_signal_history.end()-2000);
    g_observation_status="Свежих котировок для проверки: "+std::to_string(fresh_count)+"/"+std::to_string(g_items.size())+" (возраст до 20 минут, МСК). История не зависит от всплывающих уведомлений.";
    if(g_history_dirty && g_observation_writable)try {
        storage::atomic_write(storage::data_directory()/"signal_history.csv",serialize_signal_history(g_signal_history));g_history_dirty=false;g_observation_error.clear();
    }catch(const std::exception& ex){g_observation_error=std::string("Новые события пока только в памяти: ")+ex.what();}
    refresh_observation_page(g_observation_page);
    if (!g_signal_settings.notifications_enabled || newly_matched.empty()) return;
    if (g_notification_icon_ready) {
        std::wstring message;
        for (const auto& [ticker, reason] : newly_matched) {
            const std::wstring line = widen(ticker + ": " + reason);
            if (!message.empty()) message += L"\n";
            if (message.size() + line.size() >= std::size(g_notification_icon.szInfo)) {
                if(message.empty())message=line.substr(0,std::size(g_notification_icon.szInfo)-1);
                break;
            }
            message += line;
        }
        g_notification_icon.uFlags = NIF_INFO;
        g_notification_icon.dwInfoFlags = NIIF_INFO;
        const wchar_t title[] = L"Новый сигнал MOEX Watch";
        std::copy_n(title, std::min(std::size(title), std::size(g_notification_icon.szInfoTitle)) - 1,
                    g_notification_icon.szInfoTitle);
        g_notification_icon.szInfoTitle[std::size(g_notification_icon.szInfoTitle) - 1] = L'\0';
        const auto amount = std::min(message.size(), std::size(g_notification_icon.szInfo) - 1);
        std::copy_n(message.begin(), amount, g_notification_icon.szInfo);
        g_notification_icon.szInfo[amount] = L'\0';
        if (Shell_NotifyIconW(NIM_MODIFY, &g_notification_icon) == FALSE) FlashWindow(g_window, TRUE);
    } else {
        FlashWindow(g_window, TRUE);
        MessageBeep(MB_ICONINFORMATION);
    }
}

void activate_page(int page) {
    g_active_page = std::clamp(page, 0, 5);
    if(g_observation_page){ShowWindow(g_observation_page,g_active_page==5?SW_SHOW:SW_HIDE);if(g_active_page==5)refresh_observation_page(g_observation_page);}
    for (const auto control : g_market_controls) ShowWindow(control, g_active_page == 0 ? SW_SHOW : SW_HIDE);
    for (const auto control : g_trade_controls) ShowWindow(control, g_active_page == 1 ? SW_SHOW : SW_HIDE);
    for (const auto control : g_portfolio_controls) ShowWindow(control, g_active_page == 2 ? SW_SHOW : SW_HIDE);
    for (const auto control : g_signal_controls) ShowWindow(control, g_active_page == 3 ? SW_SHOW : SW_HIDE);
    for (const auto control : g_backtest_controls) ShowWindow(control, g_active_page == 4 ? SW_SHOW : SW_HIDE);
    for (const auto control : g_global_controls) ShowWindow(control, SW_SHOW);
    InvalidateRect(g_window, nullptr, TRUE);
}

void add_ticker() {
    std::string ticker = narrow(read_edit(g_ticker_edit));
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (!valid_ticker(ticker)) { MessageBoxW(g_window, L"Введите тикер латиницей, например SBER.", L"Некорректный тикер", MB_ICONWARNING); return; }
    if (g_items.size() >= 30) { MessageBoxW(g_window, L"В списке может быть не более 30 тикеров.", L"Лимит списка", MB_ICONINFORMATION); return; }
    if (std::any_of(g_items.begin(), g_items.end(), [&](const auto& x) { return x.ticker == ticker; })) {
        MessageBoxW(g_window, L"Этот тикер уже есть в списке.", L"Список наблюдения", MB_ICONINFORMATION); return;
    }
    Instrument item; item.ticker = ticker; item.quote.ticker = ticker;
    g_observer.forget_ticker(ticker);
    g_items.push_back(std::move(item)); g_selected = g_items.size() - 1;
    SendMessageW(g_backtest_ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(ticker).c_str()));
    SendMessageW(g_port_ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(ticker).c_str()));
    SendMessageW(g_port_ticker, CB_SETCURSEL, static_cast<WPARAM>(g_items.size() - 1), 0);
    SendMessageW(g_backtest_ticker, CB_SETCURSEL, static_cast<WPARAM>(g_items.size() - 1), 0);
    SetWindowTextW(g_ticker_edit, L""); save_watchlist(); fill_list(); fill_signals(); g_signal_baseline_ready = false; refresh_data();
}

void set_target() {
    const int selected = selected_instrument();
    if (selected < 0 || static_cast<std::size_t>(selected) >= g_items.size()) return;
    const auto text = narrow(read_edit(g_target_edit));
    if (text.empty()) g_items[static_cast<std::size_t>(selected)].target.reset();
    else {
        try {
            std::string normalized = text;
            std::replace(normalized.begin(), normalized.end(), ',', '.');
            std::size_t used = 0; const double value = std::stod(normalized, &used);
            if (used != normalized.size() || !std::isfinite(value) || value <= 0.0) throw std::invalid_argument("target");
            g_items[static_cast<std::size_t>(selected)].target = value;
        } catch (...) { MessageBoxW(g_window, L"Введите положительную цену. Для очистки оставьте поле пустым.", L"Некорректная цель", MB_ICONWARNING); return; }
    }
    save_watchlist(); fill_list(); fill_signals(); g_signal_baseline_ready = false;
}

void remove_selected() {
    const int selected = selected_instrument();
    if (selected < 0 || static_cast<std::size_t>(selected) >= g_items.size()) return;
    const auto portfolio = calculate_portfolio(g_portfolio_transactions, {});
    if (portfolio && std::any_of(portfolio->positions.begin(), portfolio->positions.end(), [&](const auto& position) {
            return position.ticker == g_items[static_cast<std::size_t>(selected)].ticker && position.quantity > 0.0;
        })) {
        MessageBoxW(g_window, L"Тикер нельзя убрать из наблюдения, пока по нему есть открытая позиция.", L"Открытая позиция", MB_ICONINFORMATION);
        return;
    }
    if (g_dividend_window) {
        if (static_cast<std::size_t>(selected) == g_dividend_instrument) DestroyWindow(g_dividend_window);
        else if (static_cast<std::size_t>(selected) < g_dividend_instrument) --g_dividend_instrument;
    }
    g_observer.forget_ticker(g_items[static_cast<std::size_t>(selected)].ticker);
    g_items.erase(g_items.begin() + selected);
    SendMessageW(g_port_ticker, CB_DELETESTRING, static_cast<WPARAM>(selected), 0);
    if (!g_items.empty()) SendMessageW(g_port_ticker, CB_SETCURSEL, static_cast<WPARAM>(std::min(static_cast<std::size_t>(selected), g_items.size() - 1)), 0);
    SendMessageW(g_backtest_ticker, CB_DELETESTRING, static_cast<WPARAM>(selected), 0);
    if (!g_items.empty()) SendMessageW(g_backtest_ticker, CB_SETCURSEL, static_cast<WPARAM>(std::min(static_cast<std::size_t>(selected), g_items.size() - 1)), 0);
    if (g_items.empty()) g_selected = 0; else g_selected = std::min(static_cast<std::size_t>(selected), g_items.size() - 1);
    save_watchlist(); fill_list(); fill_signals(); g_signal_baseline_ready = false; InvalidateRect(g_window, nullptr, TRUE);
}

enum class ChartPanel { Price, Comparison, Volume, Rsi };
ChartPanel g_chart_panel = ChartPanel::Price;
ChartPanel g_dividend_chart_panel = ChartPanel::Price;
int g_dividend_chart_sessions = 66;

HWND create_chart_tabs(HWND parent, int id, int x, int y, int width) {
    HWND tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_SINGLELINE,
        x, y, width, 30, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(tabs, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    const wchar_t* names[] = {L"Цена", L"Сравнение с IMOEX", L"Объёмы", L"RSI"};
    for (int i = 0; i < 4; ++i) {
        TCITEMW tab{}; tab.mask = TCIF_TEXT; tab.pszText = const_cast<wchar_t*>(names[i]);
        TabCtrl_InsertItem(tabs, i, &tab);
    }
    TabCtrl_SetCurSel(tabs, 0);
    return tabs;
}

void draw_chart(HDC dc, RECT area, std::size_t instrument_index, ChartPanel panel, int sessions) {
    FillRect(dc, &area, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    SetBkMode(dc, TRANSPARENT);
    if (instrument_index >= g_items.size() || g_items[instrument_index].candles.empty()) {
        DrawTextW(dc, L"Нет данных для графика. Выберите акцию и нажмите «Обновить».", -1,
            &area, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }
    const auto& item = g_items[instrument_index];
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
        const auto comparison = normalized_comparison_series(candles, g_imoex_candles, first);
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
            (g_imoex_stale ? " | IMOEX устарел" : ""), gray);
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

void fill_dividend_list() {
    if (!g_dividend_list || g_dividend_instrument >= g_items.size()) return;
    ListView_DeleteAllItems(g_dividend_list);
    const auto& item = g_items[g_dividend_instrument];
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
        const int row_index = ListView_InsertItem(g_dividend_list, &row);
        const auto set = [&](int column, std::wstring value) { ListView_SetItemText(g_dividend_list, row_index, column, value.data()); };
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
    if (g_dividend_info) {
        std::string info = "Обновлено: " + (item.dividends_checked_at.empty() ? "ещё не загружено" : item.dividends_checked_at) +
            (item.dividends_stale ? " | сохранённые данные, актуальность не подтверждена" : "") +
            (item.dividends.empty() ? " | событий в полученных таблицах нет" : "") +
            "\r\nSmart-Lab и таблица ЛУКОЙЛ. Двойной щелчок — открыть источник (столбец «Эмитент» — официальный сайт).\r\n¹ Расчёт T+1 по опубликованному расписанию MOEX 2026; вне покрытия — н/д. ² До налогов. Прогноз и рекомендация не гарантируют выплату.";
        if (!item.dividend_note.empty()) info += "\r\n" + item.dividend_note;
        if (!item.dividend_error.empty()) info += "\r\nОбновление не удалось: " + item.dividend_error;
        SetWindowTextW(g_dividend_info, widen(info).c_str());
    }
    apply_table_sort(g_dividend_list,ID_DIVIDENDS_LIST);
}

void refresh_dividend_events() {
    if (!g_dividend_window || g_dividend_instrument >= g_items.size()) return;
    const std::string ticker = g_items[g_dividend_instrument].ticker;
    if (g_dividend_info) SetWindowTextW(g_dividend_info, L"Загружаю открытый календарь дивидендов…");
    const HWND target = g_window;
    const auto request = ++g_dividend_request;
    EnableWindow(g_dividend_refresh, FALSE);
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

LRESULT CALLBACK dividends_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        g_dividend_window = window;
        g_dividend_info = CreateWindowExW(0, L"STATIC", L"Загружаю открытый календарь дивидендов…", WS_CHILD | WS_VISIBLE | SS_LEFT,
            12, 10, 1000, 92, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        g_dividend_refresh = CreateWindowExW(0, L"BUTTON", L"Обновить события", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1015, 8, 160, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIVIDENDS_REFRESH)), GetModuleHandleW(nullptr), nullptr);
        g_dividend_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            12, 110, 1140, 165, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIVIDENDS_LIST)), GetModuleHandleW(nullptr), nullptr);
        ListView_SetExtendedListViewStyle(g_dividend_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        const wchar_t* headers[] = {L"Дата отсечки", L"Последний день покупки¹", L"На акцию", L"Доходность²", L"Статус", L"Сверка с эмитентом", L"Купить до (источник)", L"Источник", L"Эмитент", L"Основание расчёта¹"};
        const int widths[] = {115, 180, 125, 160, 230, 280, 170, 340, 340, 460};
        for (int i = 0; i < 10; ++i) {
            LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(headers[i]); column.cx = widths[i];
            ListView_InsertColumn(g_dividend_list, i, &column);
        }
        if (g_font) {
            SendMessageW(g_dividend_info, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
            SendMessageW(g_dividend_list, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
            SendMessageW(g_dividend_refresh, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
        }
        g_dividend_chart_tabs = create_chart_tabs(window, ID_DIV_CHART_TABS, 12, 289, 800);
        g_dividend_chart_range = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            870, 289, 280, 180, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIV_CHART_RANGE)), GetModuleHandleW(nullptr), nullptr);
        if (g_font) SendMessageW(g_dividend_chart_range, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
        const wchar_t* ranges[] = {L"1 месяц (~22 сессии)", L"3 месяца (~66 сессий)", L"1 год (~252 сессии)", L"Вся загруженная история"};
        for (const auto* range : ranges) SendMessageW(g_dividend_chart_range, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(range));
        g_dividend_chart_panel = ChartPanel::Price; g_dividend_chart_sessions = 66;
        SendMessageW(g_dividend_chart_range, CB_SETCURSEL, 1, 0);
        fill_dividend_list(); refresh_dividend_events();
        return 0;
    }
    case WM_SIZE: {
        const int width = LOWORD(lparam);
        if (g_dividend_info) MoveWindow(g_dividend_info, 12, 10, width - 185, 92, TRUE);
        if (g_dividend_refresh) MoveWindow(g_dividend_refresh, width - 172, 8, 160, 28, TRUE);
        if (g_dividend_list) MoveWindow(g_dividend_list, 12, 110, width - 24, 165, TRUE);
        if (g_dividend_chart_tabs) MoveWindow(g_dividend_chart_tabs, 12, 289, width - 315, 30, TRUE);
        if (g_dividend_chart_range) MoveWindow(g_dividend_chart_range, width - 292, 289, 280, 180, TRUE);
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_DIVIDENDS_REFRESH) { refresh_dividend_events(); return 0; }
        if (LOWORD(wparam) == ID_DIV_CHART_RANGE && HIWORD(wparam) == CBN_SELCHANGE) {
            const int ranges[] = {22, 66, 252, 0};
            const auto index = SendMessageW(g_dividend_chart_range, CB_GETCURSEL, 0, 0);
            if (index >= 0 && index < 4) g_dividend_chart_sessions = ranges[index];
            InvalidateRect(window, nullptr, TRUE); return 0;
        }
        break;
    case WM_NOTIFY: {
        const auto* notice = reinterpret_cast<const NMHDR*>(lparam);
        if(notice->idFrom==ID_DIVIDENDS_LIST && notice->code==LVN_COLUMNCLICK) {
            auto& sort=g_table_sorts[ID_DIVIDENDS_LIST];const int column=reinterpret_cast<NMLISTVIEW*>(lparam)->iSubItem;
            if(sort.column==column)sort.ascending=!sort.ascending;else{sort.column=column;sort.ascending=true;}
            apply_table_sort(g_dividend_list,ID_DIVIDENDS_LIST);return 0;
        }
        if (notice->idFrom == ID_DIVIDENDS_LIST && notice->code == NM_DBLCLK) {
            const auto* click = reinterpret_cast<const NMITEMACTIVATE*>(lparam);
            if (g_dividend_instrument < g_items.size() && click->iItem >= 0 &&
                static_cast<std::size_t>(click->iItem) < g_items[g_dividend_instrument].dividends.size()) {
                const auto event_index=row_data(g_dividend_list,click->iItem);
                if(!event_index || *event_index>=g_items[g_dividend_instrument].dividends.size())return 0;
                const auto& event = g_items[g_dividend_instrument].dividends[*event_index];
                const auto& url = click->iSubItem == 8 ? event.issuer_url : event.source_url;
                if (url.starts_with("https://smart-lab.ru/") || url.starts_with("https://lukoil.ru/"))
                    ShellExecuteW(window, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            return 0;
        }
        if (notice->idFrom == ID_DIV_CHART_TABS && notice->code == TCN_SELCHANGE) {
            g_dividend_chart_panel = static_cast<ChartPanel>(std::clamp(TabCtrl_GetCurSel(g_dividend_chart_tabs), 0, 3));
            InvalidateRect(window, nullptr, TRUE); return 0;
        }
        break;
    }
    case WM_DIVIDENDS_READY: {
        std::unique_ptr<DividendFetchResult> result(reinterpret_cast<DividendFetchResult*>(lparam));
        if (wparam != g_dividend_request) return 0;
        EnableWindow(g_dividend_refresh, TRUE);
        const auto it = std::find_if(g_items.begin(), g_items.end(), [&](const auto& item) { return item.ticker == result->ticker; });
        const bool is_current = it != g_items.end() && g_dividend_instrument < g_items.size() &&
            g_items[g_dividend_instrument].ticker == result->ticker;
        if (it != g_items.end()) {
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
                g_dividend_instrument = static_cast<std::size_t>(it - g_items.begin());
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
        const auto old_font = g_font ? SelectObject(dc, g_font) : nullptr;
        draw_chart(dc, chart, g_dividend_instrument, g_dividend_chart_panel, g_dividend_chart_sessions);
        if (g_font) SelectObject(dc, old_font);
        EndPaint(window, &ps); return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize.x = 1000; limits->ptMinTrackSize.y = 760; return 0;
    }
    case WM_NCDESTROY:
        ++g_dividend_request;
        g_dividend_chart_tabs = nullptr; g_dividend_chart_range = nullptr;
        g_dividend_window = nullptr; g_dividend_list = nullptr; g_dividend_info = nullptr; g_dividend_refresh = nullptr;
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void open_dividends_window() {
    const int selected = selected_instrument();
    if (selected < 0 || static_cast<std::size_t>(selected) >= g_items.size()) {
        MessageBoxW(g_window, L"Сначала выберите акцию в списке.", L"Дивиденды", MB_ICONINFORMATION);
        return;
    }
    g_dividend_instrument = static_cast<std::size_t>(selected);
    try { load_dividend_cache(g_items[g_dividend_instrument]); }
    catch (const std::exception& ex) { g_items[g_dividend_instrument].dividend_note = ex.what(); }
    if (g_dividend_window) {
        SetWindowLongPtrW(g_dividend_window, GWLP_USERDATA, static_cast<LONG_PTR>(g_dividend_instrument));
        SetWindowTextW(g_dividend_window, (L"Дивидендная аналитика — " + widen(g_items[g_dividend_instrument].ticker)).c_str());
        fill_dividend_list(); InvalidateRect(g_dividend_window, nullptr, TRUE); ShowWindow(g_dividend_window, SW_RESTORE);
        SetForegroundWindow(g_dividend_window); refresh_dividend_events(); return;
    }
    g_dividend_window = CreateWindowExW(0, L"MOEXWatchDividendWindow", L"Дивидендная аналитика",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 840, g_window, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!g_dividend_window) {
        MessageBoxW(g_window, L"Не удалось открыть окно дивидендной аналитики.", L"Ошибка", MB_ICONERROR);
        return;
    }
    SetWindowTextW(g_dividend_window, (L"Дивидендная аналитика — " + widen(g_items[g_dividend_instrument].ticker)).c_str());
    ShowWindow(g_dividend_window, SW_SHOW); UpdateWindow(g_dividend_window);
}

void add_control(HWND parent, const wchar_t* type, const wchar_t* caption, DWORD style, int x, int y, int w, int height, int id) {
    HWND control = CreateWindowExW(0, type, caption, WS_CHILD | WS_VISIBLE | style, x, y, w, height, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    if (control) {
        if (id == ID_REFRESH) g_global_controls.push_back(control);
        else if (g_building_signal_page) g_signal_controls.push_back(control);
        else if (g_building_backtest_page) g_backtest_controls.push_back(control);
        else if (g_building_portfolio_page) g_portfolio_controls.push_back(control);
        else if (g_building_trade_page) g_trade_controls.push_back(control);
        else g_market_controls.push_back(control);
    }
}


struct BacktestUiResult {
    std::string text;
    bool failed{};
};

void start_backtest() {
    bool expected = false;
    if (!g_backtest_busy.compare_exchange_strong(expected, true)) return;
    const auto selected = SendMessageW(g_backtest_ticker, CB_GETCURSEL, 0, 0);
    if (selected < 0 || static_cast<std::size_t>(selected) >= g_items.size()) {
        g_backtest_busy = false;
        MessageBoxW(g_window, L"Сначала добавьте акцию в список наблюдения и обновите данные.", L"Тест стратегии", MB_ICONWARNING);
        return;
    }
    const auto capital = parse_finite_number(narrow(read_edit(g_backtest_capital)));
    const auto commission = parse_finite_number(narrow(read_edit(g_backtest_fee)));
    if (!capital || *capital <= 0.0 || !commission || *commission < 0.0 || *commission > 10.0) {
        g_backtest_busy = false;
        MessageBoxW(g_window, L"Укажите капитал больше нуля и комиссию за одну сторону от 0 до 10%.", L"Проверьте параметры", MB_ICONWARNING);
        return;
    }
    const auto item = g_items[static_cast<std::size_t>(selected)];
    if (item.candles.empty() || item.candles_stale) {
        g_backtest_busy = false;
        MessageBoxW(g_window, L"Сначала нажмите «Обновить» и дождитесь загрузки дневной истории выбранной акции.", L"Нет истории акции", MB_ICONWARNING);
        return;
    }
    EnableWindow(g_backtest_run, FALSE);
    SetWindowTextW(g_backtest_result, L"Загружаю историю IMOEX и рассчитываю результаты…");
    const auto settings = g_signal_settings;
    const auto target = item.target;
    const auto capital_value = *capital;
    const auto commission_value = *commission;
    const HWND target_window = g_window;
    std::thread([item, settings, target, capital_value, commission_value, target_window]() {
        auto result = std::make_unique<BacktestUiResult>();
        try {
            const auto index = fetch_index_candles();
            const auto summary = run_backtest(item.candles, index, settings, target, capital_value, commission_value);
            if (!summary) throw std::runtime_error("Недостаточно общих торговых дат или заданы неверные параметры.");
            std::ostringstream out;
            out << std::fixed << std::setprecision(2)
                << "Акция: " << item.ticker << "    Сравнение: IMOEX (удержание)\r\n"
                << "Общий период: " << summary->start_date << " — " << summary->end_date << "    Начальный капитал: " << format_price(summary->starting_capital) << " ₽\r\n"
                << "Стратегия: " << format_price(summary->strategy_final_value) << " ₽  |  доходность " << summary->strategy_return_percent << "%  |  закрытых сделок: " << summary->completed_trades << "  |  макс. просадка: " << summary->strategy_max_drawdown_percent << "%\r\n"
                << "IMOEX:     " << format_price(summary->benchmark_final_value) << " ₽  |  доходность " << summary->benchmark_return_percent << "%\r\n\r\n"
                << "Комиссия " << commission_value << "% учтена при каждой покупке и продаже. Для обоих вариантов показана оценка после комиссии. Сигнал на закрытии исполняется по открытию следующей общей сессии.";
            result->text = out.str();
        } catch (const std::exception& ex) {
            result->failed = true;
            result->text = std::string("Не удалось выполнить тест: ") + ex.what();
        }
        auto* raw = result.release();
        if (!PostMessageW(target_window, WM_BACKTEST_READY, 0, reinterpret_cast<LPARAM>(raw))) delete raw;
    }).detach();
}

struct BackupResult { std::filesystem::path path; std::string error; };
void start_backup() {
    if (!IsWindowEnabled(g_backup_button)) return;
    // Flush the UI-owned state before the worker takes a consistent disk snapshot.
    // If loading failed, preserve the original files for recovery instead.
    if (!g_storage_blocked) { save_watchlist(); save_trades(); save_portfolio(); save_position_levels(); save_signal_settings(); }
    EnableWindow(g_backup_button, FALSE);
    const HWND target=g_window;
    std::thread([target] {
        auto result=std::make_unique<BackupResult>();
        try { result->path=storage::create_backup(storage::data_directory()); }
        catch(const std::exception& ex) { result->error=ex.what(); }
        auto* raw=result.release();
        if(!PostMessageW(target,WM_BACKUP_READY,0,reinterpret_cast<LPARAM>(raw)))delete raw;
    }).detach();
}

std::optional<std::filesystem::path> select_operations_csv(bool save) {
    std::vector<wchar_t> buffer(32768,L'\0');
    if(save) { const std::wstring name=L"MOEXWatch-operations.csv"; std::copy(name.begin(),name.end(),buffer.begin()); }
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=g_window;
    dialog.lpstrFilter=L"CSV UTF-8 (*.csv)\0*.csv\0Все файлы (*.*)\0*.*\0";
    dialog.lpstrFile=buffer.data();dialog.nMaxFile=static_cast<DWORD>(buffer.size());dialog.lpstrDefExt=L"csv";
    dialog.lpstrTitle=save?L"Экспорт операций портфеля":L"Выберите CSV операций для просмотра";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    const BOOL ok=save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog);
    if(!ok){if(CommDlgExtendedError()!=0)throw std::runtime_error("Не удалось открыть диалог выбора файла");return {};}
    return std::filesystem::path(buffer.data());
}

void export_portfolio_operations() {
    try {
        if(g_storage_blocked)throw std::runtime_error("Сначала устраните ошибку загрузки журналов, чтобы не экспортировать неполный портфель");
        const auto path=select_operations_csv(true);if(!path)return;
        auto target=std::filesystem::weakly_canonical(*path).wstring();
        auto root=std::filesystem::weakly_canonical(storage::data_directory()).wstring();
        const auto lowercase=[](std::wstring& value){for(auto& c:value)c=static_cast<wchar_t>(std::towlower(c));};lowercase(target);lowercase(root);
        if(target==root || target.starts_with(root+L"\\"))throw std::runtime_error("Выберите папку вне APPDATA/MOEXWatch: экспорт не должен заменять рабочие данные или резервные копии");
        storage::atomic_write(*path,export_portfolio_csv(g_portfolio_transactions));
        MessageBoxW(g_window,(L"Операции экспортированы:\n"+path->wstring()+L"\nФормат: CSV UTF-8, разделитель ;, количество указано в акциях.").c_str(),L"Экспорт CSV",MB_ICONINFORMATION);
    }catch(const std::exception& ex){MessageBoxW(g_window,widen(ex.what()).c_str(),L"Экспорт не выполнен",MB_ICONERROR);}
}

void import_portfolio_operations() {
    try {
        if(g_storage_blocked)throw std::runtime_error("Сначала устраните ошибку хранения пользовательских данных");
        const auto path=select_operations_csv(false);if(!path)return;
        const auto csv=storage::read_text(*path);if(!csv)throw std::runtime_error("CSV-файл не найден");
        // Synchronize existing state without changing its content before opening a preview.
        save_watchlist();save_trades();save_portfolio();save_position_levels();save_signal_settings();
        if(g_storage_blocked)throw std::runtime_error("Не удалось сохранить текущие записи; импорт отменён");
        const auto root=storage::data_directory();const auto original=storage::read_text(root/"portfolio_transactions.csv");
        std::filesystem::path backup;
        const bool imported=review_portfolio_import(g_window,g_font,*csv,g_portfolio_transactions,[&](const auto& candidate){
            if(storage::read_text(root/"portfolio_transactions.csv")!=original)throw std::runtime_error("Журнал изменился после открытия просмотра. Закройте окно и повторите импорт.");
            backup=storage::create_backup(root);
            if(storage::read_text(root/"portfolio_transactions.csv")!=original)throw std::runtime_error("Журнал изменился во время резервирования. Повторите импорт.");
            storage::save_portfolio(root,candidate);
            g_portfolio_transactions=candidate;
        });
        if(imported){fill_portfolio();MessageBoxW(g_window,(L"Операции добавлены. Исходные данные сохранены:\n"+backup.wstring()+L"\nДля котировок новых тикеров добавьте их в список наблюдения.").c_str(),L"Импорт завершён",MB_ICONINFORMATION);}
    }catch(const std::exception& ex){MessageBoxW(g_window,widen(ex.what()).c_str(),L"Импорт не выполнен",MB_ICONERROR);}
}

void load_observations() {
    const auto failed=[](const std::exception& ex){g_observation_writable=false;if(!g_observation_error.empty())g_observation_error+="; ";g_observation_error+=std::string(ex.what())+". Исходный файл сохранён; новые события только в памяти.";};
    try{if(const auto bytes=storage::read_text(storage::data_directory()/"price_alerts.csv"))g_observer.configure(parse_price_alerts(*bytes));}catch(const std::exception& ex){failed(ex);}
    try{if(const auto bytes=storage::read_text(storage::data_directory()/"signal_history.csv"))g_signal_history=parse_signal_history(*bytes);}catch(const std::exception& ex){failed(ex);}
    refresh_observation_page(g_observation_page);
}

void create_controls(HWND window) {
    g_font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                            16, 8, 400, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TABS)), GetModuleHandleW(nullptr), nullptr);
    g_backup_button=CreateWindowExW(0,L"BUTTON",L"Резервная копия",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        1150,9,155,28,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BACKUP)),GetModuleHandleW(nullptr),nullptr);
    if(g_font)SendMessageW(g_backup_button,WM_SETFONT,reinterpret_cast<WPARAM>(g_font),TRUE);
    TCITEMW tab{}; tab.mask = TCIF_TEXT;
    tab.pszText = const_cast<wchar_t*>(L"Аналитика"); TabCtrl_InsertItem(g_tabs, 0, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Закрытые сделки"); TabCtrl_InsertItem(g_tabs, 1, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Портфель"); TabCtrl_InsertItem(g_tabs, 2, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Сигналы"); TabCtrl_InsertItem(g_tabs, 3, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Тест стратегии"); TabCtrl_InsertItem(g_tabs, 4, &tab);
    tab.pszText = const_cast<wchar_t*>(L"История сигналов"); TabCtrl_InsertItem(g_tabs, 5, &tab);
    TabCtrl_SetCurSel(g_tabs, 0);
    if (g_font) SendMessageW(g_tabs, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);

    add_control(window, L"STATIC", L"Тикер", 0, 16, 61, 48, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 66, 57, 105, 28, ID_TICKER);
    g_ticker_edit = GetDlgItem(window, ID_TICKER);
    add_control(window, L"BUTTON", L"Добавить", BS_PUSHBUTTON, 180, 57, 95, 28, ID_ADD);
    add_control(window, L"STATIC", L"Целевая цена, ₽", 0, 290, 61, 125, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 420, 57, 105, 28, ID_TARGET);
    g_target_edit = GetDlgItem(window, ID_TARGET);
    add_control(window, L"BUTTON", L"Сохранить цель", BS_PUSHBUTTON, 535, 57, 125, 28, ID_TARGET_SET);
    add_control(window, L"BUTTON", L"Удалить акцию", BS_PUSHBUTTON, 670, 57, 115, 28, ID_REMOVE);
    add_control(window, L"BUTTON", L"Обновить", BS_PUSHBUTTON, 795, 57, 105, 28, ID_REFRESH);
    add_control(window, L"BUTTON", L"Дивиденды", BS_PUSHBUTTON, 914, 57, 125, 28, ID_DIVIDENDS_OPEN);
    add_control(window, L"STATIC", L"Данные MOEX ISS могут отображаться с задержкой до 15 минут. Это приложение не отправляет сделки.", 0, 16, 94, 950, 22, 0);
    g_status = CreateWindowExW(0, L"STATIC", L"Готово", WS_CHILD | WS_VISIBLE, 16, 119, 940, 24, window, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_market_controls.push_back(g_status);
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
    g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                             16, 184, 550, 630, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LIST)), GetModuleHandleW(nullptr), nullptr);
    g_market_controls.push_back(g_list);
    ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* headers[] = {L"Тикер", L"Цена", L"Изм.%", L"Цель", L"RSI14", L"Статус"};
    const int widths[] = {65, 76, 68, 70, 60, 130};
    for (int i = 0; i < 6; ++i) { LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = const_cast<wchar_t*>(headers[i]); col.cx = widths[i]; ListView_InsertColumn(g_list, i, &col); }
    if (g_font) SendMessageW(g_list, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_analysis = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 610, 184, 690, 245, window, nullptr, GetModuleHandleW(nullptr), nullptr);
    g_market_controls.push_back(g_analysis);
    if (g_font) SendMessageW(g_analysis, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_chart_tabs = create_chart_tabs(window, ID_CHART_TABS, 610, 435, 690);
    g_market_controls.push_back(g_chart_tabs);

    g_building_trade_page = true;
    add_control(window, L"STATIC", L"Тикер", 0, 16, 59, 45, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 64, 55, 100, 28, ID_TRADE_TICKER);
    g_trade_ticker = GetDlgItem(window, ID_TRADE_TICKER);
    add_control(window, L"STATIC", L"Количество", 0, 180, 59, 65, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 248, 55, 95, 28, ID_TRADE_QUANTITY);
    g_trade_quantity = GetDlgItem(window, ID_TRADE_QUANTITY);
    add_control(window, L"STATIC", L"Покупка, ₽", 0, 360, 59, 75, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 439, 55, 100, 28, ID_TRADE_BUY);
    g_trade_buy = GetDlgItem(window, ID_TRADE_BUY);
    add_control(window, L"STATIC", L"Продажа, ₽", 0, 555, 59, 75, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 635, 55, 100, 28, ID_TRADE_SELL);
    g_trade_sell = GetDlgItem(window, ID_TRADE_SELL);
    add_control(window, L"STATIC", L"Комиссия, ₽", 0, 16, 99, 100, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 118, 95, 105, 28, ID_TRADE_COMMISSION);
    g_trade_commission = GetDlgItem(window, ID_TRADE_COMMISSION);
    add_control(window, L"STATIC", L"Налоговая ставка, %", 0, 238, 99, 175, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 430, 95, 90, 28, ID_TRADE_TAX);
    g_trade_tax = GetDlgItem(window, ID_TRADE_TAX);
    add_control(window, L"BUTTON", L"Записать сделку", BS_PUSHBUTTON, 540, 95, 140, 28, ID_TRADE_ADD);
    add_control(window, L"BUTTON", L"Удалить выбранную", BS_PUSHBUTTON, 692, 95, 155, 28, ID_TRADE_DELETE);
    add_control(window, L"STATIC", L"Налог рассчитывается ориентировочно по введённой ставке; пустое поле означает 0%. Проверьте ставку для вашей ситуации.",
                0, 16, 130, 1200, 22, 0);
    g_trade_summary = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 153, 1200, 26, window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TRADE_SUMMARY)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_trade_summary, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_trade_controls.push_back(g_trade_summary);
    g_trade_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                   16, 184, 1260, 620, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TRADE_LIST)), GetModuleHandleW(nullptr), nullptr);
    g_trade_controls.push_back(g_trade_list);
    ListView_SetExtendedListViewStyle(g_trade_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* trade_headers[] = {L"Тикер", L"Кол-во", L"Покупка, ₽", L"Продажа, ₽", L"Валовая, ₽", L"Комиссия, ₽", L"Налог, %", L"Налог, ₽", L"Чистая, ₽"};
    const int trade_widths[] = {75, 75, 105, 105, 110, 110, 90, 100, 110};
    for (int i = 0; i < 9; ++i) { LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = const_cast<wchar_t*>(trade_headers[i]); col.cx = trade_widths[i]; ListView_InsertColumn(g_trade_list, i, &col); }
    if (g_font) SendMessageW(g_trade_list, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_building_trade_page = false;

    g_building_portfolio_page = true;
    add_control(window, L"STATIC", L"Дата", 0, 16, 59, 42, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 60, 55, 105, 28, ID_PORT_DATE);
    g_port_date = GetDlgItem(window, ID_PORT_DATE);
    add_control(window, L"STATIC", L"Тикер", 0, 178, 59, 48, 24, 0);
    g_port_ticker = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        229, 55, 115, 220, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_TICKER)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_port_ticker, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_portfolio_controls.push_back(g_port_ticker);
    add_control(window, L"STATIC", L"Операция", 0, 356, 59, 68, 24, 0);
    g_port_side = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        428, 55, 110, 100, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_SIDE)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_port_side, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_portfolio_controls.push_back(g_port_side);
    SendMessageW(g_port_side, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Покупка"));
    SendMessageW(g_port_side, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Продажа"));
    SendMessageW(g_port_side, CB_SETCURSEL, 0, 0);
    add_control(window, L"STATIC", L"Количество", 0, 550, 59, 72, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 626, 55, 95, 28, ID_PORT_QUANTITY);
    g_port_quantity = GetDlgItem(window, ID_PORT_QUANTITY);
    add_control(window, L"STATIC", L"Цена, ₽", 0, 16, 99, 58, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 78, 95, 105, 28, ID_PORT_PRICE);
    g_port_price = GetDlgItem(window, ID_PORT_PRICE);
    add_control(window, L"STATIC", L"Комиссия, ₽", 0, 203, 99, 100, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 308, 95, 105, 28, ID_PORT_COMMISSION);
    g_port_commission = GetDlgItem(window, ID_PORT_COMMISSION);
    g_port_add = CreateWindowExW(0, L"BUTTON", L"Записать операцию", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        435, 95, 155, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_ADD)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_port_add, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_portfolio_controls.push_back(g_port_add);
    add_control(window,L"BUTTON",L"Импорт CSV",BS_PUSHBUTTON,620,95,125,28,ID_PORT_IMPORT);
    add_control(window,L"BUTTON",L"Экспорт CSV",BS_PUSHBUTTON,758,95,125,28,ID_PORT_EXPORT);
    add_control(window, L"STATIC", L"Тикер выбирается из списка наблюдения. В себестоимость покупки включается комиссия; продажа списывается по средней цене.",
        0, 16, 132, 1220, 22, 0);
    g_port_summary = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 157, 1240, 24, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_SUMMARY)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_port_summary, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_portfolio_controls.push_back(g_port_summary);
    g_port_positions = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        16, 185, 1260, 220, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_POSITIONS)), GetModuleHandleW(nullptr), nullptr);
    g_portfolio_controls.push_back(g_port_positions);
    ListView_SetExtendedListViewStyle(g_port_positions, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* position_headers[] = {L"Тикер",L"Акций",L"Средняя, ₽",L"Котировка, ₽",L"Стоимость, ₽",L"Доля, %",L"Бумажная прибыль, ₽",L"Бумажная, %",L"Зафиксировано, ₽",L"Стоп-лосс, ₽",L"Тейк-профит, ₽"};
    const int position_widths[] = {75,80,105,110,115,90,155,100,145,110,125};
    for (int i=0;i<11;++i) { LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.pszText=const_cast<wchar_t*>(position_headers[i]); col.cx=position_widths[i]; ListView_InsertColumn(g_port_positions,i,&col); }
    add_control(window, L"STATIC", L"Стоп-лосс, ₽", 0, 16, 418, 105, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 121, 414, 110, 28, ID_POSITION_STOP);
    g_position_stop = GetDlgItem(window, ID_POSITION_STOP);
    add_control(window, L"STATIC", L"Тейк-профит, ₽", 0, 248, 418, 115, 24, 0);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 367, 414, 110, 28, ID_POSITION_TAKE);
    g_position_take = GetDlgItem(window, ID_POSITION_TAKE);
    g_position_levels_save = CreateWindowExW(0, L"BUTTON", L"Сохранить уровни", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        494, 414, 155, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_POSITION_LEVELS_SAVE)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_position_levels_save, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_portfolio_controls.push_back(g_position_levels_save);
    add_control(window, L"STATIC", L"Выберите позицию; пустое поле отключает уровень. Линии также показываются на графике выбранной акции.",
        0, 664, 418, 610, 24, 0);
    add_control(window, L"STATIC", L"Журнал операций", 0, 16, 466, 160, 24, 0);
    g_port_delete = CreateWindowExW(0, L"BUTTON", L"Удалить выбранную", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        1060, 458, 170, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_DELETE)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_port_delete, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_portfolio_controls.push_back(g_port_delete);
    g_port_transactions_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        16, 492, 1260, 300, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PORT_TRANSACTIONS)), GetModuleHandleW(nullptr), nullptr);
    g_portfolio_controls.push_back(g_port_transactions_list);
    ListView_SetExtendedListViewStyle(g_port_transactions_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* operation_headers[] = {L"Дата",L"Тикер",L"Операция",L"Акций",L"Цена, ₽",L"Комиссия, ₽"};
    const int operation_widths[] = {110,95,120,100,120,130};
    for (int i=0;i<6;++i) { LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.pszText=const_cast<wchar_t*>(operation_headers[i]); col.cx=operation_widths[i]; ListView_InsertColumn(g_port_transactions_list,i,&col); }
    if (g_font) SendMessageW(g_port_transactions_list, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_building_portfolio_page = false;

    g_building_signal_page = true;
    add_control(window, L"BUTTON", L"RSI ниже", BS_AUTOCHECKBOX, 16, 59, 100, 28, ID_SIG_OVERSOLD);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 120, 55, 65, 28, ID_SIG_OVERSOLD_VALUE);
    g_signal_oversold_value = GetDlgItem(window, ID_SIG_OVERSOLD_VALUE);
    add_control(window, L"BUTTON", L"RSI выше", BS_AUTOCHECKBOX, 215, 59, 100, 28, ID_SIG_OVERBOUGHT);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 319, 55, 65, 28, ID_SIG_OVERBOUGHT_VALUE);
    g_signal_overbought_value = GetDlgItem(window, ID_SIG_OVERBOUGHT_VALUE);
    add_control(window, L"BUTTON", L"Цена выше SMA200", BS_AUTOCHECKBOX, 16, 97, 175, 28, ID_SIG_SMA);
    add_control(window, L"BUTTON", L"Цена рядом с целью", BS_AUTOCHECKBOX, 205, 97, 185, 28, ID_SIG_TARGET);
    add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 397, 95, 65, 28, ID_SIG_TARGET_VALUE);
    g_signal_target_value = GetDlgItem(window, ID_SIG_TARGET_VALUE);
    add_control(window, L"STATIC", L"%", 0, 466, 98, 20, 24, 0);
    add_control(window, L"STATIC", L"Совпадение", 0, 495, 98, 90, 24, 0);
    add_control(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 585, 94, 135, 100, ID_SIG_MODE);
    SendMessageW(GetDlgItem(window, ID_SIG_MODE), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Любое условие"));
    SendMessageW(GetDlgItem(window, ID_SIG_MODE), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Все условия"));
    add_control(window, L"BUTTON", L"Всплывающие уведомления", BS_AUTOCHECKBOX, 738, 97, 240, 28, ID_SIG_NOTIFY);
    add_control(window, L"BUTTON", L"Сохранить условия", BS_PUSHBUTTON, 990, 95, 145, 28, ID_SIG_SAVE);
    add_control(window, L"BUTTON", L"Автообновление", BS_AUTOCHECKBOX, 16, 135, 155, 28, ID_SIG_AUTO_REFRESH);
    add_control(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 176, 132, 135, 100, ID_SIG_INTERVAL);
    g_signal_interval = GetDlgItem(window, ID_SIG_INTERVAL);
    const wchar_t* interval_names[] = {L"15 минут", L"30 минут", L"60 минут"};
    for (const auto* interval : interval_names)
        SendMessageW(g_signal_interval, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(interval));
    add_control(window, L"STATIC", L"Проверка работает, пока приложение открыто; уведомления появляются при новых совпадениях.",
                0, 326, 137, 800, 22, 0);
    g_signal_summary = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 170, 1200, 25, window,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SIG_SUMMARY)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_signal_summary, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_signal_controls.push_back(g_signal_summary);
    g_signal_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                    16, 202, 1260, 600, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SIG_LIST)), GetModuleHandleW(nullptr), nullptr);
    g_signal_controls.push_back(g_signal_list);
    ListView_SetExtendedListViewStyle(g_signal_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t* signal_headers[] = {L"Тикер", L"Цена, ₽", L"RSI14", L"SMA200, ₽", L"Цель, ₽", L"Почему совпало"};
    const int signal_widths[] = {90, 100, 90, 120, 100, 520};
    for (int i = 0; i < 6; ++i) { LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = const_cast<wchar_t*>(signal_headers[i]); col.cx = signal_widths[i]; ListView_InsertColumn(g_signal_list, i, &col); }
    if (g_font) SendMessageW(g_signal_list, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_building_signal_page = false;

    g_building_backtest_page = true;
    add_control(window, L"STATIC", L"Акция", 0, 16, 61, 55, 24, 0);
    g_backtest_ticker = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        78, 55, 145, 220, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BACKTEST_TICKER)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_backtest_ticker, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_backtest_controls.push_back(g_backtest_ticker);
    add_control(window, L"STATIC", L"Капитал, ₽", 0, 242, 61, 85, 24, 0);
    add_control(window, L"EDIT", L"100000", WS_BORDER | ES_AUTOHSCROLL, 330, 55, 110, 28, ID_BACKTEST_CAPITAL);
    g_backtest_capital = GetDlgItem(window, ID_BACKTEST_CAPITAL);
    add_control(window, L"STATIC", L"Комиссия за сторону, %", 0, 458, 61, 168, 24, 0);
    add_control(window, L"EDIT", L"0.05", WS_BORDER | ES_AUTOHSCROLL, 630, 55, 80, 28, ID_BACKTEST_FEE);
    g_backtest_fee = GetDlgItem(window, ID_BACKTEST_FEE);
    g_backtest_run = CreateWindowExW(0, L"BUTTON", L"Запустить тест", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        920, 55, 145, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BACKTEST_RUN)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_backtest_run, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_backtest_controls.push_back(g_backtest_run);
    add_control(window, L"STATIC", L"Тестируются сохранённые условия со вкладки «Сигналы». Сделки — на следующем открытии после сигнала на закрытии.",
                0, 16, 96, 1190, 24, 0);
    g_backtest_result = CreateWindowExW(0, L"STATIC", L"Выберите акцию из списка наблюдения и нажмите «Запустить тест стратегии».",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 145, 1220, 200, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BACKTEST_RESULT)), GetModuleHandleW(nullptr), nullptr);
    if (g_font) SendMessageW(g_backtest_result, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_backtest_controls.push_back(g_backtest_result);
    g_building_backtest_page = false;
    ObservationBindings bindings;
    bindings.rules=[] {return g_observer.rules();};bindings.events=[] {return g_signal_history;};
    bindings.tickers=[] {std::vector<std::string> tickers;for(const auto& item:g_items)tickers.push_back(item.ticker);return tickers;};
    bindings.error=[] {return g_observation_status+" "+g_observation_error;};
    bindings.save_rules=[](const auto& rules){
        if(!g_observation_writable)throw std::runtime_error("Сначала восстановите повреждённые файлы наблюдений из резервной копии");
        storage::atomic_write(storage::data_directory()/"price_alerts.csv",serialize_price_alerts(rules));g_observer.configure(rules);
    };
    bindings.open_ticker=[](const std::string& ticker){const auto it=std::find_if(g_items.begin(),g_items.end(),[&](const auto& item){return item.ticker==ticker;});if(it==g_items.end()){MessageBoxW(g_window,L"Бумаги нет в наблюдении. Добавьте тикер на вкладке «Аналитика».",L"История сигналов",MB_ICONINFORMATION);return;}g_filter=WatchFilter::All;SendMessageW(GetDlgItem(g_window,ID_FILTER),CB_SETCURSEL,0,0);g_selected=static_cast<std::size_t>(it-g_items.begin());fill_list();TabCtrl_SetCurSel(g_tabs,0);activate_page(0);};
    g_observation_page=create_observation_page(window,g_font,std::move(bindings));
    if(!g_observation_page)MessageBoxW(window,L"Не удалось создать вкладку истории сигналов.",L"MOEXWatch",MB_ICONERROR);
    activate_page(0);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        g_window = window; create_controls(window); load_signal_settings(); synchronize_signal_controls();
        update_auto_refresh_timer(); load_watchlist(); load_observations(); load_trades(); load_portfolio(); load_position_levels(); load_market_cache(g_items, g_imoex_candles, g_imoex_error); fill_list(); fill_trades(); fill_signals(); fill_portfolio();
        for (const auto& item : g_items) {
            SendMessageW(g_backtest_ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(item.ticker).c_str()));
            SendMessageW(g_port_ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(item.ticker).c_str()));
        }
        if (!g_items.empty()) { SendMessageW(g_backtest_ticker, CB_SETCURSEL, 0, 0); SendMessageW(g_port_ticker, CB_SETCURSEL, 0, 0); }
        SetWindowTextW(g_port_date, widen(today_date()).c_str());
        add_notification_icon(); refresh_data(); return 0;
    case WM_SIZE: {
        const int width = LOWORD(lparam), height = HIWORD(lparam);
        if(g_observation_page)MoveWindow(g_observation_page,16,100,width-32,height-116,TRUE);
        if (g_tabs) MoveWindow(g_tabs, 16, 8, width - 195, 34, TRUE);
        if(g_backup_button)MoveWindow(g_backup_button, width-171, 9, 155, 28, TRUE);
        if (g_list) MoveWindow(g_list, 16, 184, std::max(360, width * 44 / 100), height - 204, TRUE);
        if (g_analysis) MoveWindow(g_analysis, width * 46 / 100, 184, width * 52 / 100 - 16, 245, TRUE);
        if (g_chart_tabs) MoveWindow(g_chart_tabs, width * 46 / 100, 435, width - width * 46 / 100 - 16, 30, TRUE);
        if (g_trade_summary) MoveWindow(g_trade_summary, 16, 153, width - 32, 26, TRUE);
        if (g_trade_list) MoveWindow(g_trade_list, 16, 184, width - 32, height - 204, TRUE);
        if (g_signal_summary) MoveWindow(g_signal_summary, 16, 170, width - 32, 25, TRUE);
        if (g_signal_list) MoveWindow(g_signal_list, 16, 202, width - 32, height - 222, TRUE);
        if (g_backtest_result) MoveWindow(g_backtest_result, 16, 145, width - 32, 260, TRUE);
        if (g_port_summary) MoveWindow(g_port_summary, 16, 157, width - 32, 24, TRUE);
        if (g_port_positions) MoveWindow(g_port_positions, 16, 185, width - 32, 220, TRUE);
        if (g_port_transactions_list) MoveWindow(g_port_transactions_list, 16, 492, width - 32, std::max(100, height - 512), TRUE);
        if (g_port_delete) MoveWindow(g_port_delete, std::max(870, width - 190), 458, 170, 28, TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize.x = 1180;
        limits->ptMinTrackSize.y = 760;
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_ADD: add_ticker(); return 0;
        case ID_TARGET_SET: set_target(); return 0;
        case ID_REMOVE: remove_selected(); return 0;
        case ID_REFRESH: refresh_data(); return 0;
        case ID_BACKUP: start_backup(); return 0;
        case ID_DIVIDENDS_OPEN: open_dividends_window(); return 0;
        case ID_TRADE_ADD: add_trade(); return 0;
        case ID_TRADE_DELETE: delete_trade(); return 0;
        case ID_BACKTEST_RUN: start_backtest(); return 0;
        case ID_PORT_ADD: add_portfolio_transaction(); return 0;
        case ID_PORT_IMPORT: import_portfolio_operations(); return 0;
        case ID_PORT_EXPORT: export_portfolio_operations(); return 0;
        case ID_PORT_DELETE: delete_portfolio_transaction(); return 0;
        case ID_POSITION_LEVELS_SAVE: save_selected_position_levels(); return 0;
        case ID_SIG_SAVE:
            if (read_signal_settings_from_controls()) MessageBoxW(window, L"Условия сохранены. Уведомления будут приходить при появлении новых совпадений; текущие сигналы не дублируются.", L"Сигналы", MB_ICONINFORMATION);
            return 0;
        case ID_FILTER:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                g_filter = static_cast<WatchFilter>(SendMessageW(GetDlgItem(window, ID_FILTER), CB_GETCURSEL, 0, 0));
                fill_list();
            }
            return 0;
        case ID_RANGE:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                const int ranges[] = {22, 66, 252, 0};
                const auto index = SendMessageW(GetDlgItem(window, ID_RANGE), CB_GETCURSEL, 0, 0);
                if (index >= 0 && index < 4) g_chart_sessions = ranges[index];
                InvalidateRect(window, nullptr, TRUE);
            }
            return 0;
        }
        break;
    case WM_NOTIFY:
        if(reinterpret_cast<NMHDR*>(lparam)->code==LVN_COLUMNCLICK) {
            const auto* click=reinterpret_cast<NMLISTVIEW*>(lparam);const int id=static_cast<int>(click->hdr.idFrom);
            if(id==ID_LIST||id==ID_TRADE_LIST||id==ID_PORT_POSITIONS||id==ID_PORT_TRANSACTIONS||id==ID_SIG_LIST){auto& sort=g_table_sorts[id];if(sort.column==click->iSubItem)sort.ascending=!sort.ascending;else{sort.column=click->iSubItem;sort.ascending=true;}apply_table_sort(click->hdr.hwndFrom,id);if(id==ID_PORT_POSITIONS)fill_position_level_editor();return 0;}
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_CHART_TABS && reinterpret_cast<NMHDR*>(lparam)->code == TCN_SELCHANGE) {
            g_chart_panel = static_cast<ChartPanel>(std::clamp(TabCtrl_GetCurSel(g_chart_tabs), 0, 3));
            InvalidateRect(window, nullptr, TRUE); return 0;
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_TABS && reinterpret_cast<NMHDR*>(lparam)->code == TCN_SELCHANGE) {
            activate_page(TabCtrl_GetCurSel(g_tabs)); return 0;
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_LIST && reinterpret_cast<NMHDR*>(lparam)->code == LVN_ITEMCHANGED) {
            if (g_filling) return 0;
            const int selected = selected_instrument();
            if (selected >= 0 && static_cast<std::size_t>(selected) < g_items.size()) { g_selected = static_cast<std::size_t>(selected); update_analysis(); }
        }
        if (reinterpret_cast<NMHDR*>(lparam)->idFrom == ID_PORT_POSITIONS &&
            reinterpret_cast<NMHDR*>(lparam)->code == LVN_ITEMCHANGED) {
            if(g_filling)return 0;
            const auto* changed = reinterpret_cast<const NMLISTVIEW*>(lparam);
            if ((changed->uNewState & LVIS_SELECTED) != 0) fill_position_level_editor();
            return 0;
        }
        return 0;
    case WM_FETCH_PROGRESS: {
        const std::wstring status = wparam == 0
            ? std::wstring(L"Загрузка дневной истории IMOEX…")
            : std::wstring(L"Загрузка котировок и истории: акция ") + std::to_wstring(wparam) + L" из " + std::to_wstring(lparam);
        SetWindowTextW(g_status, status.c_str()); return 0;
    }
    case WM_TIMER:
        if (wparam == TIMER_AUTO_REFRESH && g_signal_settings.auto_refresh_enabled) { refresh_data(); return 0; }
        break;
    case WM_DATA_READY: {
        std::unique_ptr<MarketRefreshResult> data(reinterpret_cast<MarketRefreshResult*>(lparam));
        g_busy = false; EnableWindow(GetDlgItem(window, ID_REFRESH), TRUE);
        if (!data->imoex_candles.empty()) {
            g_imoex_candles = std::move(data->imoex_candles);
            g_imoex_stale = false;
            g_imoex_error.clear();
        } else {
            g_imoex_stale = true;
            g_imoex_error = std::move(data->imoex_error);
        }
        for (auto& fetched : data->instruments) {
            const auto it = std::find_if(g_items.begin(), g_items.end(), [&](const auto& existing) { return existing.ticker == fetched.ticker; });
            if (it != g_items.end()) {
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
        const auto loaded = std::count_if(g_items.begin(), g_items.end(), [](const auto& item) { return item.quote.last && !item.quote_stale; });
        std::wstring status = L"Котировки загружены: " + std::to_wstring(loaded) + L" из " + std::to_wstring(g_items.size()) + L". ";
        status += g_imoex_stale ? L"IMOEX не обновлён; сравнение использует старые данные. " : L"IMOEX обновлён. ";
        status += L"Подробности — в выбранной акции.";
        SetWindowTextW(g_status, status.c_str());
        if (g_dividend_window) { fill_dividend_list(); InvalidateRect(g_dividend_window, nullptr, TRUE); }
        fill_list(); fill_signals(); fill_portfolio(); notify_new_signals(); return 0;
    }
    case WM_BACKUP_READY: {
        std::unique_ptr<BackupResult> result(reinterpret_cast<BackupResult*>(lparam));
        EnableWindow(g_backup_button,TRUE);
        if(!result->error.empty())MessageBoxW(window,widen("Резервная копия не создана: "+result->error).c_str(),L"MOEXWatch",MB_ICONERROR);
        else {
            MessageBoxW(window,(L"Копия создана и проверена:\n"+result->path.wstring()+
                L"\n\nДля восстановления закройте приложение и следуйте RESTORE.txt в этой папке.").c_str(),L"Резервная копия",MB_ICONINFORMATION);
            ShellExecuteW(window,L"open",result->path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        }
        return 0;
    }
    case WM_DIVIDENDS_READY:
        if (!g_dividend_window || !PostMessageW(g_dividend_window, message, wparam, lparam))
            delete reinterpret_cast<DividendFetchResult*>(lparam);
        return 0;
    case WM_BACKTEST_READY: {
        std::unique_ptr<BacktestUiResult> result(reinterpret_cast<BacktestUiResult*>(lparam));
        g_backtest_busy = false;
        EnableWindow(g_backtest_run, TRUE);
        const auto display = widen(result->text);
        SetWindowTextW(g_backtest_result, display.c_str());
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(window, &ps);
        if (g_active_page == 0) {
            RECT client{}; GetClientRect(window, &client);
            RECT chart{client.left + client.right * 46 / 100, 475, client.right - 16, client.bottom - 20};
            const auto old_font = SelectObject(dc, g_font);
            draw_chart(dc, chart, g_selected, g_chart_panel, g_chart_sessions);
            SelectObject(dc, old_font);
        }
        EndPaint(window, &ps); return 0;
    }
    case WM_DESTROY:
        KillTimer(window, TIMER_AUTO_REFRESH);
        save_watchlist(); save_trades(); save_portfolio(); save_position_levels(); save_signal_settings();
        if (g_notification_icon_ready) Shell_NotifyIconW(NIM_DELETE, &g_notification_icon);
        if (g_font) DeleteObject(g_font); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES}; InitCommonControlsEx(&controls);
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = window_proc; wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MOEXWatchWindow"; wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return 1;
    WNDCLASSEXW dividend_wc{}; dividend_wc.cbSize = sizeof(dividend_wc);
    dividend_wc.lpfnWndProc = dividends_window_proc; dividend_wc.hInstance = instance;
    dividend_wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    dividend_wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    dividend_wc.lpszClassName = L"MOEXWatchDividendWindow";
    if (!RegisterClassExW(&dividend_wc)) return 1;
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"MOEX Watch 0.6.2 — аналитика и портфель", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1340, 870, nullptr, nullptr, instance, nullptr);
    if (!window) return 2;
    ShowWindow(window, show); UpdateWindow(window);
    MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return static_cast<int>(msg.wParam);
}

#else
#include <iostream>
int main() {
    using namespace moexwatch;
    const std::string fixture = "begin;open;high;low;close;volume\n2026-10-01;100;103;99;102;1000\n2026-10-02;102;104;101;103;1200\n";
    const auto candles = parse_candles_csv(fixture);
    const auto result = analyze(candles);
    std::cout << "MOEXWatch portable core check\n"
              << "Parsed candles: " << candles.size() << "\n"
              << "Change: " << (result.period_change_percent ? format_price(*result.period_change_percent) + "%" : "n/a") << "\n"
              << "For the Windows 10 GUI build use build_windows.ps1.\n";
}
#endif
