#include "market_core.hpp"

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
#include <shellapi.h>
#include <winhttp.h>

using namespace moexwatch;

namespace {
constexpr UINT WM_DATA_READY = WM_APP + 10;
constexpr UINT WM_FETCH_PROGRESS = WM_APP + 11;
constexpr UINT WM_BACKTEST_READY = WM_APP + 12;
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
constexpr UINT_PTR TIMER_AUTO_REFRESH = 1;

struct Instrument {
    std::string ticker;
    std::optional<double> target;
    Quote quote;
    std::vector<Candle> candles;
    std::string error;
    bool quote_stale{true};
    bool candles_stale{true};
};

HWND g_window{};
HWND g_list{};
HWND g_ticker_edit{};
HWND g_target_edit{};
HWND g_status{};
HWND g_analysis{};
HWND g_tabs{};
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
std::atomic<bool> g_busy{false};
std::size_t g_selected{};
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
std::unordered_map<std::string, bool> g_previous_signal_matches;
bool g_signal_baseline_ready = false;
NOTIFYICONDATAW g_notification_icon{};
bool g_notification_icon_ready = false;
WatchFilter g_filter = WatchFilter::All;
int g_chart_sessions = 66;
int selected_instrument() {
    const int row = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
    return row >= 0 && static_cast<std::size_t>(row) < g_visible.size()
        ? static_cast<int>(g_visible[static_cast<std::size_t>(row)]) : -1;
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

std::filesystem::path watchlist_path() {
    const DWORD required = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
    std::wstring appdata;
    if (required > 1) {
        appdata.resize(required);
        const DWORD written = GetEnvironmentVariableW(L"APPDATA", appdata.data(), required);
        if (written > 0 && written < required) appdata.resize(written); else appdata.clear();
    }
    const std::filesystem::path folder = (appdata.empty() ? std::filesystem::path(L".") : std::filesystem::path(appdata)) / L"MOEXWatch";
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    return folder / L"watchlist.csv";
}

std::filesystem::path trades_path() {
    const DWORD required = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
    std::wstring appdata;
    if (required > 1) {
        appdata.resize(required);
        const DWORD written = GetEnvironmentVariableW(L"APPDATA", appdata.data(), required);
        if (written > 0 && written < required) appdata.resize(written); else appdata.clear();
    }
    const std::filesystem::path folder = (appdata.empty() ? std::filesystem::path(L".") : std::filesystem::path(appdata)) / L"MOEXWatch";
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    return folder / L"trades.csv";
}

std::filesystem::path portfolio_path() {
    const DWORD required = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
    std::wstring appdata;
    if (required > 1) {
        appdata.resize(required);
        const DWORD written = GetEnvironmentVariableW(L"APPDATA", appdata.data(), required);
        if (written > 0 && written < required) appdata.resize(written); else appdata.clear();
    }
    const std::filesystem::path folder = (appdata.empty() ? std::filesystem::path(L".") : std::filesystem::path(appdata)) / L"MOEXWatch";
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    return folder / L"portfolio_transactions.csv";
}

std::filesystem::path position_levels_path() {
    const DWORD required = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
    std::wstring appdata;
    if (required > 1) {
        appdata.resize(required);
        const DWORD written = GetEnvironmentVariableW(L"APPDATA", appdata.data(), required);
        if (written > 0 && written < required) appdata.resize(written); else appdata.clear();
    }
    const std::filesystem::path folder = (appdata.empty() ? std::filesystem::path(L".") : std::filesystem::path(appdata)) / L"MOEXWatch";
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    return folder / L"position_levels.csv";
}

std::filesystem::path signals_path() {
    const DWORD required = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
    std::wstring appdata;
    if (required > 1) {
        appdata.resize(required);
        const DWORD written = GetEnvironmentVariableW(L"APPDATA", appdata.data(), required);
        if (written > 0 && written < required) appdata.resize(written); else appdata.clear();
    }
    const std::filesystem::path folder = (appdata.empty() ? std::filesystem::path(L".") : std::filesystem::path(appdata)) / L"MOEXWatch";
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    return folder / L"signals.ini";
}

std::optional<double> parse_finite_number(std::string value);

void save_signal_settings() {
    std::ofstream out(signals_path(), std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << std::setprecision(12)
        << "rsi_oversold_enabled=" << (g_signal_settings.rsi_oversold_enabled ? 1 : 0) << '\n'
        << "rsi_oversold_below=" << g_signal_settings.rsi_oversold_below << '\n'
        << "rsi_overbought_enabled=" << (g_signal_settings.rsi_overbought_enabled ? 1 : 0) << '\n'
        << "rsi_overbought_above=" << g_signal_settings.rsi_overbought_above << '\n'
        << "above_sma200_enabled=" << (g_signal_settings.above_sma200_enabled ? 1 : 0) << '\n'
        << "near_target_enabled=" << (g_signal_settings.near_target_enabled ? 1 : 0) << '\n'
        << "near_target_percent=" << g_signal_settings.near_target_percent << '\n'
        << "require_all=" << (g_signal_settings.require_all ? 1 : 0) << '\n'
        << "notifications_enabled=" << (g_signal_settings.notifications_enabled ? 1 : 0) << '\n'
        << "auto_refresh_enabled=" << (g_signal_settings.auto_refresh_enabled ? 1 : 0) << '\n'
        << "auto_refresh_minutes=" << g_signal_settings.auto_refresh_minutes << '\n';
}

void load_signal_settings() {
    std::ifstream in(signals_path(), std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        const auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        const auto key = line.substr(0, sep), value = line.substr(sep + 1);
        const bool enabled = value == "1";
        if (key == "rsi_oversold_enabled") g_signal_settings.rsi_oversold_enabled = enabled;
        else if (key == "rsi_overbought_enabled") g_signal_settings.rsi_overbought_enabled = enabled;
        else if (key == "above_sma200_enabled") g_signal_settings.above_sma200_enabled = enabled;
        else if (key == "near_target_enabled") g_signal_settings.near_target_enabled = enabled;
        else if (key == "require_all") g_signal_settings.require_all = enabled;
        else if (key == "notifications_enabled") g_signal_settings.notifications_enabled = enabled;
        else if (key == "auto_refresh_enabled") g_signal_settings.auto_refresh_enabled = enabled;
        else if (key == "auto_refresh_minutes") {
            const auto number = parse_finite_number(value);
            if (number && std::floor(*number) == *number && (*number == 15 || *number == 30 || *number == 60))
                g_signal_settings.auto_refresh_minutes = static_cast<int>(*number);
        }
        else if (key == "rsi_oversold_below" || key == "rsi_overbought_above" || key == "near_target_percent") {
            const auto number = parse_finite_number(value);
            if (!number) continue;
            if (key == "rsi_oversold_below") g_signal_settings.rsi_oversold_below = *number;
            else if (key == "rsi_overbought_above") g_signal_settings.rsi_overbought_above = *number;
            else g_signal_settings.near_target_percent = *number;
        }
    }
    if (!valid_signal_settings(g_signal_settings)) g_signal_settings = SignalSettings{};
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
    std::ofstream out(trades_path(), std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << "ticker;quantity;buy_price;sell_price;commission;tax_rate_percent\n";
    out << std::setprecision(15);
    for (const auto& trade : g_trades)
        out << trade.ticker << ';' << trade.quantity << ';' << trade.buy_price << ';' << trade.sell_price << ';'
            << trade.commission << ';' << trade.tax_rate_percent << '\n';
}

void load_trades() {
    std::ifstream in(trades_path(), std::ios::binary);
    if (!in) return;
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto rows = parse_csv(content);
    if (rows.empty() || rows.front().size() < 6 || rows.front()[0] != "ticker") return;
    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].size() < 6) continue;
        const auto quantity = parse_finite_number(rows[i][1]);
        const auto buy = parse_finite_number(rows[i][2]);
        const auto sell = parse_finite_number(rows[i][3]);
        const auto commission = parse_finite_number(rows[i][4]);
        const auto tax = parse_finite_number(rows[i][5]);
        if (!quantity || !buy || !sell || !commission || !tax) continue;
        Trade trade{rows[i][0], *quantity, *buy, *sell, *commission, *tax};
        if (valid_trade(trade)) g_trades.push_back(std::move(trade));
    }
}

void save_portfolio() {
    std::ofstream out(portfolio_path(), std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << "date;ticker;side;quantity;price;commission\n" << std::setprecision(15);
    for (const auto& transaction : g_portfolio_transactions)
        out << transaction.date << ';' << transaction.ticker << ';'
            << (transaction.side == PortfolioSide::Buy ? "BUY" : "SELL") << ';'
            << transaction.quantity << ';' << transaction.price << ';' << transaction.commission << '\n';
}

void load_portfolio() {
    std::ifstream in(portfolio_path(), std::ios::binary);
    if (!in) return;
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto rows = parse_csv(content);
    if (rows.empty() || rows.front().size() < 6 || rows.front()[0] != "date") return;
    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].size() < 6) continue;
        const auto quantity = parse_finite_number(rows[i][3]);
        const auto price = parse_finite_number(rows[i][4]);
        const auto commission = parse_finite_number(rows[i][5]);
        if (!quantity || !price || !commission) continue;
        const auto side = rows[i][2] == "BUY" ? PortfolioSide::Buy :
            (rows[i][2] == "SELL" ? PortfolioSide::Sell : PortfolioSide::Buy);
        if (rows[i][2] != "BUY" && rows[i][2] != "SELL") continue;
        PortfolioTransaction transaction{rows[i][0], rows[i][1], side, *quantity, *price, *commission};
        if (valid_portfolio_transaction(transaction)) g_portfolio_transactions.push_back(std::move(transaction));
    }
    std::stable_sort(g_portfolio_transactions.begin(), g_portfolio_transactions.end(),
        [](const auto& a, const auto& b) { return a.date < b.date; });
}

void save_position_levels() {
    std::ofstream out(position_levels_path(), std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << "ticker;opened_on;stop_loss;take_profit\n" << std::setprecision(15);
    for (const auto& levels : g_position_levels) {
        if (!valid_position_levels(levels) || (!levels.stop_loss && !levels.take_profit)) continue;
        out << levels.ticker << ';' << levels.opened_on << ';';
        if (levels.stop_loss) out << *levels.stop_loss;
        out << ';';
        if (levels.take_profit) out << *levels.take_profit;
        out << '\n';
    }
}

void load_position_levels() {
    std::ifstream in(position_levels_path(), std::ios::binary);
    if (!in) return;
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto rows = parse_csv(content);
    if (rows.empty() || rows.front().size() < 4 || rows.front()[0] != "ticker") return;
    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].size() < 4) continue;
        const auto stop = rows[i][2].empty() ? std::optional<double>{} : parse_finite_number(rows[i][2]);
        const auto take = rows[i][3].empty() ? std::optional<double>{} : parse_finite_number(rows[i][3]);
        if ((!rows[i][2].empty() && !stop) || (!rows[i][3].empty() && !take)) continue;
        PositionLevels levels{rows[i][0], rows[i][1], stop, take};
        if (valid_position_levels(levels)) g_position_levels.push_back(std::move(levels));
    }
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
    std::ofstream out(watchlist_path(), std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << "# ticker;target\n";
    for (const auto& item : g_items) {
        out << item.ticker << ';';
        if (item.target) out << std::setprecision(12) << *item.target;
        out << '\n';
    }
}

void load_watchlist() {
    std::ifstream in(watchlist_path(), std::ios::binary);
    std::string line;
    while (std::getline(in, line) && g_items.size() < 30) {
        if (line.empty() || line[0] == '#') continue;
        const auto sep = line.find(';');
        std::string ticker = line.substr(0, sep);
        std::transform(ticker.begin(), ticker.end(), ticker.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (!valid_ticker(ticker) || std::any_of(g_items.begin(), g_items.end(), [&](const auto& x) { return x.ticker == ticker; })) continue;
        Instrument item; item.ticker = ticker; item.quote.ticker = ticker;
        if (sep != std::string::npos && sep + 1 < line.size()) {
            try { const auto target = std::stod(line.substr(sep + 1)); if (target > 0.0) item.target = target; } catch (...) {}
        }
        g_items.push_back(std::move(item));
    }
    if (g_items.empty()) {
        for (const char* ticker : {"GAZP", "LKOH", "SBER", "ROSN", "OZON", "T", "YDEX", "NVTK", "SMLT", "VTBR"}) {
            Instrument item; item.ticker = ticker; item.quote.ticker = ticker; g_items.push_back(std::move(item));
        }
        save_watchlist();
    }
}

struct InternetHandle {
    HINTERNET value{};
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    explicit InternetHandle(HINTERNET h = nullptr) : value(h) {}
};

std::runtime_error network_error(const char* message) {
    const DWORD code = GetLastError();
    return std::runtime_error(std::string(message) + " (WinHTTP/Windows " + std::to_string(code) + ")");
}

std::string https_get(const std::wstring& path) {
    InternetHandle session(WinHttpOpen(L"MOEXWatch/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                       WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value) throw network_error("Не удалось открыть HTTPS-сессию");
    WinHttpSetTimeouts(session.value, 5000, 5000, 6000, 6000);
    InternetHandle connection(WinHttpConnect(session.value, L"iss.moex.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection.value) throw network_error("Не удалось подключиться к MOEX ISS");
    InternetHandle request(WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr,
                                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request.value) throw network_error("Не удалось создать HTTPS-запрос");
    if (!WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.value, nullptr)) throw network_error("Ошибка запроса к MOEX ISS");
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) || status < 200 || status >= 300)
        throw std::runtime_error("MOEX ISS ответил HTTP " + std::to_string(status));
    std::string body;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) throw std::runtime_error("Ошибка чтения ответа MOEX ISS");
        if (available == 0) break;
        if (body.size() + available > 2U * 1024U * 1024U) throw std::runtime_error("Ответ MOEX ISS превышает 2 МБ");
        const auto old_size = body.size();
        body.resize(old_size + available);
        DWORD received = 0;
        if (!WinHttpReadData(request.value, body.data() + old_size, available, &received)) throw std::runtime_error("Ошибка чтения ответа MOEX ISS");
        body.resize(old_size + received);
    }
    return body;
}

std::string date_from() {
    const auto day = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) - std::chrono::days{730};
    const std::chrono::year_month_day ymd{day};
    std::ostringstream out;
    out << static_cast<int>(ymd.year()) << '-' << std::setfill('0') << std::setw(2) << static_cast<unsigned>(ymd.month())
        << '-' << std::setw(2) << static_cast<unsigned>(ymd.day());
    return out.str();
}

Instrument fetch_instrument(const Instrument& previous) {
    Instrument item = previous;
    item.error.clear();
    item.quote_stale = true;
    item.candles_stale = true;
    const std::wstring base = L"/iss/engines/stock/markets/shares/boards/TQBR/securities/" + widen(item.ticker);
    try {
        const auto csv = https_get(base + L".csv?iss.meta=on&iss.only=marketdata&marketdata.columns=SECID,LAST,LASTCHANGEPRCNT,UPDATETIME&iss.delimiter=%3B&iss.dp=point&lang=en");
        const auto quote = parse_quote_csv(csv, item.ticker);
        if (quote) { item.quote = *quote; item.quote_stale = !quote->last.has_value(); }
        else item.error = "Котировки: формат CSV не распознан";
        if (quote && !quote->last) item.error = "MOEX не вернул цену последней сделки";
    } catch (const std::exception& ex) { item.error = std::string("Котировки: ") + ex.what(); }
    try {
        std::vector<Candle> candles;
        bool complete = false;
        for (std::size_t page = 0; page < 16; ++page) {
            const auto path = base + L"/candles.csv?from=" + widen(date_from()) + L"&interval=24&iss.meta=on&iss.only=candles&candles.columns=begin,open,high,low,close,volume&iss.delimiter=%3B&iss.dp=point&lang=en&start=" + std::to_wstring(candles.size());
            const auto csv = https_get(path);
            if (!has_candle_header(csv)) throw std::runtime_error("MOEX: формат страницы истории не распознан");
            auto batch = parse_candles_csv(csv);
            if (batch.empty()) { complete = true; break; }
            if (!candles.empty() && batch.front().date <= candles.back().date)
                throw std::runtime_error("MOEX повторил страницу истории");
            candles.insert(candles.end(), batch.begin(), batch.end());
        }
        if (!complete) throw std::runtime_error("История превышает лимит страниц");
        if (!candles.empty()) { item.candles = std::move(candles); item.candles_stale = false; }
        else {
            if (!item.error.empty()) item.error += "; ";
            item.error += "MOEX не вернул дневные свечи";
        }
    } catch (const std::exception& ex) {
        if (!item.error.empty()) item.error += "; ";
        item.error += std::string("График: ") + ex.what();
    }
    return item;
}

void refresh_data() {
    bool expected = false;
    if (!g_busy.compare_exchange_strong(expected, true)) return;
    EnableWindow(GetDlgItem(g_window, ID_REFRESH), FALSE);
    SetWindowTextW(g_status, L"Загружаю данные MOEX ISS…");
    const auto source = g_items;
    const HWND target = g_window;
    std::thread([source, target]() {
        auto result = std::make_unique<std::vector<Instrument>>();
        for (const auto& item : source) {
            PostMessageW(target, WM_FETCH_PROGRESS, static_cast<WPARAM>(result->size() + 1), static_cast<LPARAM>(source.size()));
            try { result->push_back(fetch_instrument(item)); }
            catch (const std::exception& ex) {
                Instrument failed = item;
                failed.error = ex.what();
                result->push_back(std::move(failed));
            }
        }
        auto* raw = result.release();
        if (!PostMessageW(target, WM_DATA_READY, 0, reinterpret_cast<LPARAM>(raw))) delete raw;
    }).detach();
}

std::string status_for(const Instrument& item) {
    if (!item.quote.last) return item.error.empty() ? "Нет котировки" : "Ошибка данных";
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
        LVITEMW row{}; row.mask = LVIF_TEXT; row.iItem = row_index;
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
         << "От максимума истории: " << percent(a.drawdown_percent) << "\r\n"
         << "Тренд по дневному закрытию: " << (a.sma200 ? (item.candles.back().close > *a.sma200 ? "выше SMA200" : "ниже/равно SMA200") : "для SMA200 нужны 200 сессий") << "\r\n"
         << "Индикаторы: " << a.technical_note << "\r\n"
         << "Котировка: " << (item.quote_stale ? "устарела или недоступна" : (item.quote.updated_at.empty() ? "время обновления не указано" : item.quote.updated_at))
         << "  |  график: " << (item.candles_stale ? "не обновлён" : "обновлён")
         << "  |  свечей: " << item.candles.size();
    if (!item.error.empty()) text << "\r\nСтатус источника: " << item.error;
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
        LVITEMW row{}; row.mask = LVIF_TEXT; row.iItem = static_cast<int>(i);
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
    std::size_t row_index = 0;
    if (portfolio) {
        for (const auto& position : portfolio->positions) {
            if (position.quantity <= 0.0) continue;
            LVITEMW row{}; row.mask = LVIF_TEXT; row.iItem = static_cast<int>(row_index);
            auto ticker = widen(position.ticker); row.pszText = ticker.data();
            ListView_InsertItem(g_port_positions, &row);
            bool stale = false;
            const auto source = std::find_if(g_items.begin(), g_items.end(), [&](const auto& item) { return item.ticker == position.ticker; });
            if (source != g_items.end()) stale = source->quote_stale;
            const auto* levels = levels_for_position(position.ticker, position.opened_on);
            const std::string values[] = {money(position.quantity), money(position.average_entry_price),
                position.current_price ? money(*position.current_price) + (stale ? " *" : "") : "—",
                position.market_value ? money(*position.market_value) : "—",
                position.unrealized_profit ? money(*position.unrealized_profit) : "—",
                percent(position.unrealized_percent), money(position.realized_profit),
                levels && levels->stop_loss ? money(*levels->stop_loss) : "—",
                levels && levels->take_profit ? money(*levels->take_profit) : "—"};
            for (int col = 1; col <= 9; ++col) {
                auto wide = widen(values[col - 1]); ListView_SetItemText(g_port_positions, static_cast<int>(row_index), col, wide.data());
            }
            ++row_index;
        }
    }
    for (std::size_t i = 0; i < g_portfolio_transactions.size(); ++i) {
        const auto& transaction = g_portfolio_transactions[i];
        LVITEMW row{}; row.mask = LVIF_TEXT; row.iItem = static_cast<int>(i);
        auto date = widen(transaction.date); row.pszText = date.data();
        ListView_InsertItem(g_port_transactions_list, &row);
        const std::string values[] = {transaction.ticker, portfolio_side_name(transaction.side),
            money(transaction.quantity), money(transaction.price), money(transaction.commission)};
        for (int col = 1; col <= 5; ++col) {
            auto wide = widen(values[col - 1]); ListView_SetItemText(g_port_transactions_list, static_cast<int>(i), col, wide.data());
        }
    }
    if (!portfolio) {
        SetWindowTextW(g_port_summary, L"В журнале есть продажа без достаточного остатка. Проверьте и удалите ошибочную операцию.");
        return;
    }
    const auto summary = L"Открытых позиций: " + std::to_wstring(portfolio->open_positions) +
        L"   Котировки: " + std::to_wstring(portfolio->priced_positions) + L"/" + std::to_wstring(portfolio->open_positions) +
        L"   Стоимость: " + widen(money(portfolio->total_market_value)) + L" ₽" +
        L"   Бумажная прибыль: " + widen(money(portfolio->total_unrealized_profit)) + L" ₽" +
        L"   Зафиксировано: " + widen(money(portfolio->total_realized_profit)) + L" ₽ (* — устаревшая цена)";
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
    const int selected = ListView_GetNextItem(g_port_transactions_list, -1, LVNI_SELECTED);
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
    const int selected = ListView_GetNextItem(g_trade_list, -1, LVNI_SELECTED);
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
    g_signal_settings = settings;
    save_signal_settings();
    update_auto_refresh_timer();
    g_signal_baseline_ready = false;
    fill_signals();
    return true;
}

void fill_signals() {
    if (!g_signal_list || !g_signal_summary) return;
    ListView_DeleteAllItems(g_signal_list);
    std::size_t count = 0;
    for (const auto& item : g_items) {
        const auto analysis = analyze(item.candles);
        const auto evaluation = evaluate_signals(analysis, item.quote.last, item.target,
            item.quote_stale, item.candles_stale, g_signal_settings);
        if (!evaluation.matched) continue;
        ++count;
        LVITEMW row{}; row.mask = LVIF_TEXT; row.iItem = static_cast<int>(count - 1);
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
        L"Сигналы считаются только по свежей котировке и истории.";
    SetWindowTextW(g_signal_summary, summary.c_str());
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
    std::unordered_map<std::string, bool> current;
    std::vector<std::pair<std::string, std::string>> newly_matched;
    for (const auto& item : g_items) {
        const auto evaluation = evaluate_signals(analyze(item.candles), item.quote.last, item.target,
            item.quote_stale, item.candles_stale, g_signal_settings);
        current[item.ticker] = evaluation.matched;
        const auto previous = g_previous_signal_matches.find(item.ticker);
        if (g_signal_baseline_ready && evaluation.matched &&
            (previous == g_previous_signal_matches.end() || !previous->second))
            newly_matched.emplace_back(item.ticker, signal_reason_text(evaluation));
    }
    g_previous_signal_matches = std::move(current);
    if (!g_signal_baseline_ready) { g_signal_baseline_ready = true; return; }
    if (!g_signal_settings.notifications_enabled || newly_matched.empty()) return;
    if (g_notification_icon_ready) {
        std::wstring message;
        for (const auto& [ticker, reason] : newly_matched) {
            const std::wstring line = widen(ticker + ": " + reason);
            if (!message.empty()) message += L"\n";
            if (message.size() + line.size() >= std::size(g_notification_icon.szInfo)) break;
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
    g_active_page = std::clamp(page, 0, 4);
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
    g_items.erase(g_items.begin() + selected);
    SendMessageW(g_port_ticker, CB_DELETESTRING, static_cast<WPARAM>(selected), 0);
    if (!g_items.empty()) SendMessageW(g_port_ticker, CB_SETCURSEL, static_cast<WPARAM>(std::min(static_cast<std::size_t>(selected), g_items.size() - 1)), 0);
    SendMessageW(g_backtest_ticker, CB_DELETESTRING, static_cast<WPARAM>(selected), 0);
    if (!g_items.empty()) SendMessageW(g_backtest_ticker, CB_SETCURSEL, static_cast<WPARAM>(std::min(static_cast<std::size_t>(selected), g_items.size() - 1)), 0);
    if (g_items.empty()) g_selected = 0; else g_selected = std::min(static_cast<std::size_t>(selected), g_items.size() - 1);
    save_watchlist(); fill_list(); fill_signals(); g_signal_baseline_ready = false; InvalidateRect(g_window, nullptr, TRUE);
}

void draw_chart(HDC dc, RECT area) {
    FillRect(dc, &area, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    if (g_items.empty() || g_selected >= g_items.size() || g_items[g_selected].candles.empty()) {
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, L"Нет данных для графика. Выберите акцию и нажмите «Обновить».", -1, &area, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }
    const auto& item = g_items[g_selected];
    const auto& candles = item.candles;
    const auto active_position = open_position_for_ticker(item.ticker);
    const auto* position_levels = active_position
        ? levels_for_position(active_position->ticker, active_position->opened_on) : nullptr;
    const auto sma20 = sma_series(candles, 20), sma50 = sma_series(candles, 50), sma200 = sma_series(candles, 200);
    const auto rsi = rsi_series(candles);
    const std::size_t count = candles.size();
    const std::size_t first = g_chart_sessions > 0 && count > static_cast<std::size_t>(g_chart_sessions)
        ? count - static_cast<std::size_t>(g_chart_sessions) : 0;
    const auto shown = count - first;
    const LONG split = area.top + (area.bottom - area.top) * 65 / 100;
    RECT plot{area.left + 62, area.top + 42, area.right - 16, split - 18};
    RECT oscillator{plot.left, split + 24, plot.right, area.bottom - 26};
    if (plot.right - plot.left < 30 || plot.bottom - plot.top < 40 || oscillator.bottom - oscillator.top < 40) return;
    SetBkMode(dc, TRANSPARENT);
    const auto text = [&](int x, int y, const std::string& value, COLORREF color) {
        SetTextColor(dc, color); const auto label = widen(value);
        TextOutW(dc, x, y, label.c_str(), static_cast<int>(label.size()));
    };
    text(area.left + 4, area.top, item.ticker + " | Дневные свечи" + (item.candles_stale ? " | УСТАРЕВШИЕ ДАННЫЕ" : ""), RGB(40, 50, 65));
    text(area.left + 4, area.top + 19, "SMA20", RGB(232, 139, 43));
    text(area.left + 80, area.top + 19, "SMA50", RGB(34, 105, 181));
    text(area.left + 160, area.top + 19, "SMA200", RGB(135, 73, 170));
    if (active_position) {
        std::string position_legend = "Позиция: средняя " + money(active_position->average_entry_price);
        if (position_levels && position_levels->stop_loss) position_legend += " | стоп " + money(*position_levels->stop_loss);
        if (position_levels && position_levels->take_profit) position_legend += " | тейк " + money(*position_levels->take_profit);
        text(area.left + 250, area.top + 19, position_legend, RGB(55, 65, 80));
    }
    double minimum = candles[first].low, maximum = candles[first].high;
    for (std::size_t i = first; i < count; ++i) {
        minimum = std::min(minimum, candles[i].low); maximum = std::max(maximum, candles[i].high);
        for (const auto* average : {&sma20, &sma50, &sma200}) {
            if ((*average)[i]) { minimum = std::min(minimum, *(*average)[i]); maximum = std::max(maximum, *(*average)[i]); }
        }
    }
    if (active_position) {
        minimum = std::min(minimum, active_position->average_entry_price);
        maximum = std::max(maximum, active_position->average_entry_price);
        if (position_levels && position_levels->stop_loss) {
            minimum = std::min(minimum, *position_levels->stop_loss);
            maximum = std::max(maximum, *position_levels->stop_loss);
        }
        if (position_levels && position_levels->take_profit) {
            minimum = std::min(minimum, *position_levels->take_profit);
            maximum = std::max(maximum, *position_levels->take_profit);
        }
    }
    const double padding = std::max((maximum - minimum) * 0.06, maximum * 0.001);
    minimum -= padding; maximum += padding;
    const auto px = [&](std::size_t i) { return static_cast<int>(plot.left + (static_cast<double>(i - first) + 0.5) * static_cast<double>(plot.right - plot.left) / static_cast<double>(shown)); };
    const auto py = [&](double value) { return static_cast<int>(plot.bottom - (value - minimum) / (maximum - minimum) * static_cast<double>(plot.bottom - plot.top)); };
    const auto ry = [&](double value) { return static_cast<int>(oscillator.bottom - value / 100.0 * static_cast<double>(oscillator.bottom - oscillator.top)); };
    HPEN grid = CreatePen(PS_SOLID, 1, RGB(225, 231, 238));
    HGDIOBJ old_pen = SelectObject(dc, grid);
    for (int k = 0; k <= 4; ++k) {
        const double price = minimum + (maximum - minimum) * static_cast<double>(k) / 4.0;
        const int y = py(price); MoveToEx(dc, plot.left, y, nullptr); LineTo(dc, plot.right, y);
        text(area.left + 1, y - 8, format_price(price), RGB(90, 99, 109));
    }
    SelectObject(dc, old_pen); DeleteObject(grid);
    const int half = std::clamp(static_cast<int>((plot.right - plot.left) / static_cast<LONG>(shown) / 3), 1, 6);
    for (std::size_t i = first; i < count; ++i) {
        const auto& candle = candles[i];
        const COLORREF color = candle.close >= candle.open ? RGB(29, 144, 100) : RGB(207, 66, 78);
        HPEN pen = CreatePen(PS_SOLID, 1, color); HBRUSH brush = CreateSolidBrush(color);
        old_pen = SelectObject(dc, pen); HGDIOBJ old_brush = SelectObject(dc, brush);
        const int x = px(i); MoveToEx(dc, x, py(candle.high), nullptr); LineTo(dc, x, py(candle.low));
        const int top = std::min(py(candle.open), py(candle.close));
        const int bottom = std::max(top + 1, std::max(py(candle.open), py(candle.close)));
        Rectangle(dc, x - half, top, x + half + 1, bottom + 1);
        SelectObject(dc, old_brush); SelectObject(dc, old_pen); DeleteObject(brush); DeleteObject(pen);
    }
    const auto draw_line = [&](const std::vector<std::optional<double>>& values, COLORREF color, bool is_rsi) {
        HPEN pen = CreatePen(PS_SOLID, 2, color); HGDIOBJ previous = SelectObject(dc, pen);
        bool started = false;
        for (std::size_t i = first; i < count; ++i) {
            if (!values[i]) { started = false; continue; }
            const int x = px(i), y = is_rsi ? ry(*values[i]) : py(*values[i]);
            if (!started) { MoveToEx(dc, x, y, nullptr); started = true; } else LineTo(dc, x, y);
        }
        SelectObject(dc, previous); DeleteObject(pen);
    };
    draw_line(sma20, RGB(232, 139, 43), false);
    draw_line(sma50, RGB(34, 105, 181), false);
    draw_line(sma200, RGB(135, 73, 170), false);
    const auto draw_position_level = [&](double value, const std::string& label, COLORREF color) {
        HPEN pen = CreatePen(PS_DASH, 1, color);
        HGDIOBJ previous = SelectObject(dc, pen);
        const int y = py(value);
        MoveToEx(dc, plot.left, y, nullptr); LineTo(dc, plot.right, y);
        SelectObject(dc, previous); DeleteObject(pen);
        text(std::max(plot.left + 4, plot.right - 150), y - 15, label + " " + money(value), color);
    };
    if (active_position) {
        draw_position_level(active_position->average_entry_price, "Средняя", RGB(70, 90, 120));
        if (position_levels && position_levels->stop_loss)
            draw_position_level(*position_levels->stop_loss, "Стоп", RGB(200, 55, 65));
        if (position_levels && position_levels->take_profit)
            draw_position_level(*position_levels->take_profit, "Тейк", RGB(25, 135, 90));
    }
    const auto fill_band = [&](int top, int bottom, COLORREF color) {
        RECT band{oscillator.left, top, oscillator.right, bottom}; HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &band, brush); DeleteObject(brush);
    };
    fill_band(oscillator.top, ry(70.0), RGB(255, 235, 236));
    fill_band(ry(30.0), oscillator.bottom, RGB(230, 247, 239));
    text(area.left + 4, split + 3, "RSI14 Уайлдера: " + optional_number(rsi.back()) + " | уровни 30 / 70", RGB(65, 70, 90));
    HPEN levels = CreatePen(PS_DASH, 1, RGB(155, 160, 170)); old_pen = SelectObject(dc, levels);
    for (double level : {0.0, 30.0, 50.0, 70.0, 100.0}) {
        const int y = ry(level); MoveToEx(dc, oscillator.left, y, nullptr); LineTo(dc, oscillator.right, y);
        text(area.left + 20, y - 7, std::to_string(static_cast<int>(level)), RGB(95, 105, 115));
    }
    SelectObject(dc, old_pen); DeleteObject(levels);
    draw_line(rsi, RGB(105, 67, 176), true);
    text(plot.left, area.bottom - 19, candles[first].date.substr(0, 10), RGB(90, 99, 109));
    text(plot.right - 98, area.bottom - 19, candles.back().date.substr(0, 10), RGB(90, 99, 109));
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

std::vector<Candle> fetch_index_candles() {
    const std::wstring base = L"/iss/engines/stock/markets/index/boards/SNDX/securities/IMOEX";
    std::vector<Candle> candles;
    bool complete = false;
    for (std::size_t page = 0; page < 16; ++page) {
        const auto path = base + L"/candles.csv?from=" + widen(date_from()) +
            L"&interval=24&iss.meta=on&iss.only=candles&candles.columns=begin,open,high,low,close,volume&iss.delimiter=%3B&iss.dp=point&lang=en&start=" + std::to_wstring(candles.size());
        const auto csv = https_get(path);
        if (!has_candle_header(csv)) throw std::runtime_error("MOEX ISS: формат истории IMOEX не распознан");
        auto batch = parse_candles_csv(csv);
        if (batch.empty()) { complete = true; break; }
        if (!candles.empty() && batch.front().date <= candles.back().date)
            throw std::runtime_error("MOEX ISS повторил страницу истории IMOEX");
        candles.insert(candles.end(), batch.begin(), batch.end());
    }
    if (!complete) throw std::runtime_error("История IMOEX превышает лимит страниц");
    if (candles.size() < 2) throw std::runtime_error("MOEX ISS не вернул достаточно дневных данных IMOEX");
    return candles;
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

void create_controls(HWND window) {
    g_font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                            16, 8, 400, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TABS)), GetModuleHandleW(nullptr), nullptr);
    TCITEMW tab{}; tab.mask = TCIF_TEXT;
    tab.pszText = const_cast<wchar_t*>(L"Аналитика"); TabCtrl_InsertItem(g_tabs, 0, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Закрытые сделки"); TabCtrl_InsertItem(g_tabs, 1, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Портфель"); TabCtrl_InsertItem(g_tabs, 2, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Сигналы"); TabCtrl_InsertItem(g_tabs, 3, &tab);
    tab.pszText = const_cast<wchar_t*>(L"Тест стратегии"); TabCtrl_InsertItem(g_tabs, 4, &tab);
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
    const wchar_t* position_headers[] = {L"Тикер",L"Акций",L"Средняя, ₽",L"Котировка, ₽",L"Стоимость, ₽",L"Бумажная прибыль, ₽",L"Бумажная, %",L"Зафиксировано, ₽",L"Стоп-лосс, ₽",L"Тейк-профит, ₽"};
    const int position_widths[] = {75,80,105,110,115,155,100,145,110,125};
    for (int i=0;i<10;++i) { LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.pszText=const_cast<wchar_t*>(position_headers[i]); col.cx=position_widths[i]; ListView_InsertColumn(g_port_positions,i,&col); }
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
    add_control(window, L"BUTTON", L"Уведомлять о новых совпадениях", BS_AUTOCHECKBOX, 738, 97, 240, 28, ID_SIG_NOTIFY);
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
    activate_page(0);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        g_window = window; create_controls(window); load_signal_settings(); synchronize_signal_controls();
        update_auto_refresh_timer(); load_watchlist(); load_trades(); load_portfolio(); load_position_levels(); fill_list(); fill_trades(); fill_signals(); fill_portfolio();
        for (const auto& item : g_items) {
            SendMessageW(g_backtest_ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(item.ticker).c_str()));
            SendMessageW(g_port_ticker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(item.ticker).c_str()));
        }
        if (!g_items.empty()) { SendMessageW(g_backtest_ticker, CB_SETCURSEL, 0, 0); SendMessageW(g_port_ticker, CB_SETCURSEL, 0, 0); }
        SetWindowTextW(g_port_date, widen(today_date()).c_str());
        add_notification_icon(); refresh_data(); return 0;
    case WM_SIZE: {
        const int width = LOWORD(lparam), height = HIWORD(lparam);
        if (g_tabs) MoveWindow(g_tabs, 16, 8, width - 32, 34, TRUE);
        if (g_list) MoveWindow(g_list, 16, 184, std::max(360, width * 44 / 100), height - 204, TRUE);
        if (g_analysis) MoveWindow(g_analysis, width * 46 / 100, 184, width * 52 / 100 - 16, 245, TRUE);
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
        case ID_TRADE_ADD: add_trade(); return 0;
        case ID_TRADE_DELETE: delete_trade(); return 0;
        case ID_BACKTEST_RUN: start_backtest(); return 0;
        case ID_PORT_ADD: add_portfolio_transaction(); return 0;
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
            const auto* changed = reinterpret_cast<const NMLISTVIEW*>(lparam);
            if ((changed->uNewState & LVIS_SELECTED) != 0) fill_position_level_editor();
            return 0;
        }
        return 0;
    case WM_FETCH_PROGRESS: {
        const auto status = L"Загрузка котировок и истории: акция " + std::to_wstring(wparam) + L" из " + std::to_wstring(lparam);
        SetWindowTextW(g_status, status.c_str()); return 0;
    }
    case WM_TIMER:
        if (wparam == TIMER_AUTO_REFRESH && g_signal_settings.auto_refresh_enabled) { refresh_data(); return 0; }
        break;
    case WM_DATA_READY: {
        std::unique_ptr<std::vector<Instrument>> data(reinterpret_cast<std::vector<Instrument>*>(lparam));
        g_busy = false; EnableWindow(GetDlgItem(window, ID_REFRESH), TRUE);
        for (auto& fetched : *data) {
            const auto it = std::find_if(g_items.begin(), g_items.end(), [&](const auto& existing) { return existing.ticker == fetched.ticker; });
            if (it != g_items.end()) {
                const auto current_target = it->target;
                *it = std::move(fetched);
                it->target = current_target;
            }
        }
        const auto loaded = std::count_if(g_items.begin(), g_items.end(), [](const auto& item) { return item.quote.last && !item.quote_stale; });
        const auto status = L"Котировки загружены: " + std::to_wstring(loaded) + L" из " + std::to_wstring(g_items.size()) + L". Подробности — в выбранной акции.";
        SetWindowTextW(g_status, status.c_str());
        fill_list(); fill_signals(); fill_portfolio(); notify_new_signals(); return 0;
    }
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
            RECT chart{client.left + client.right * 46 / 100, 445, client.right - 16, client.bottom - 20};
            const auto old_font = SelectObject(dc, g_font);
            draw_chart(dc, chart);
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
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"MOEX Watch 0.3.1 — аналитика и портфель", WS_OVERLAPPEDWINDOW,
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
