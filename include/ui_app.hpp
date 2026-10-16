#pragma once
#ifdef _WIN32
#include "app_state.hpp"
#include "ui_shell_state.hpp"
#include "ui_dividends.hpp"
#include "ui_trades.hpp"
#include "ui_charts.hpp"
#include "ui_portfolio.hpp"
#include "ui_signals.hpp"
#include "ui_backtest.hpp"
#include "market_core.hpp"
#include "app_models.hpp"
#include "storage.hpp"
#include "market_data.hpp"
#include "portfolio_csv.hpp"
#include "portfolio_import_ui.hpp"
#include "observation.hpp"
#include "observation_ui.hpp"
#include "table_sort_win.hpp"

#include <algorithm>
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
#include <utility>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <cwctype>
#include <shellapi.h>

namespace moexwatch::ui {
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

struct BackupResult { std::filesystem::path path; std::string error; };
// UI-thread owner. Pages share market/model data through this explicit owner.
// Each page keeps its own controls and transient UI state in the structs above.
class Application {
public:
    BacktestState backtest_page;
    SignalsState signals_page;
    PortfolioState portfolio_page;
    ChartsState charts_page;
    TradesState trades_page;
    ApplicationState state;
    ShellState shell;
    DividendsState dividends_page;
    TableSort* table_sort(int id);
    bool* view_updating(int id);
    bool select_instrument(std::string_view ticker, bool reveal = false);
    std::size_t chart_instrument_index() const;
    std::size_t dividend_instrument_index() const;
    void apply_market_refresh(std::uint64_t request, MarketRefreshResult result);
    void refresh_market_views();
    void apply_table_sort(HWND list,int id);
    int selected_instrument();
    std::wstring widen(std::string_view text);
    std::string narrow(std::wstring_view text);
    std::wstring read_edit(HWND control);
    void storage_failure(const char* error);
    void save_signal_settings();
    void load_signal_settings();
    std::optional<double> parse_finite_number(std::string value);
    void update_auto_refresh_timer();
    std::string money(double value);
    void save_trades();
    void load_trades();
    void save_portfolio();
    void load_portfolio();
    void save_position_levels();
    void load_position_levels();
    std::string optional_number(std::optional<double> n);
    std::string percent(std::optional<double> n);
    void save_watchlist();
    void load_watchlist();
    void refresh_data();
    std::string status_for(const Instrument& item);
    void fill_list();
    void update_analysis();
    void fill_trades();
    std::vector<Quote> portfolio_quotes();
    const PositionLevels* levels_for_position(std::string_view ticker, std::string_view opened_on);
    std::optional<PortfolioPosition> open_position_for_ticker(std::string_view ticker);
    std::string portfolio_side_name(PortfolioSide side);
    void fill_portfolio();
    void fill_position_level_editor();
    void save_selected_position_levels();
    std::string today_date();
    void add_portfolio_transaction();
    void delete_portfolio_transaction();
    void add_trade();
    void delete_trade();
    void synchronize_signal_controls();
    bool read_signal_settings_from_controls();
    void fill_signals();
    void add_notification_icon();
    std::string signal_reason_text(const SignalEvaluation& evaluation);
    void notify_new_signals();
    void activate_page(int page);
    void add_ticker();
    void set_target();
    void remove_selected();
    HWND create_chart_tabs(HWND parent, int id, int x, int y, int width);
    void draw_chart(HDC dc, RECT area, std::size_t instrument_index, ChartPanel panel, int sessions);
    void fill_dividend_list();
    void refresh_dividend_events();
    void open_dividends_window();
    void add_control(std::vector<HWND>& controls, HWND parent, const wchar_t* type, const wchar_t* caption, DWORD style, int x, int y, int w, int height, int id);
    void start_backtest();
    void start_backup();
    std::optional<std::filesystem::path> select_operations_csv(bool save);
    void export_portfolio_operations();
    void import_portfolio_operations();
    void load_observations();
    void create_controls(HWND window);
    void create_charts_controls(HWND window);
    void create_trades_controls(HWND window);
    void create_portfolio_controls(HWND window);
    void create_signals_controls(HWND window);
    void create_backtest_controls(HWND window);
    void create_observation_controls(HWND window);
    LRESULT CALLBACK handle_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT CALLBACK handle_dividends_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void resize_backtest(int width, int height);
    void resize_signals(int width, int height);
    void resize_portfolio(int width, int height);
    void resize_trades(int width, int height);
    void resize_charts(int width, int height);
    bool command_backtest(WPARAM wparam);
    bool command_portfolio(WPARAM wparam);
    bool command_trades(WPARAM wparam);
    bool command_signals(WPARAM wparam);
    void complete_backtest(LPARAM lparam);
    bool command_charts(WPARAM wparam);
    bool notify_charts(LPARAM lparam);
    void paint_charts(HDC dc,RECT client);
    static LRESULT CALLBACK window_proc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK dividends_window_proc(HWND,UINT,WPARAM,LPARAM);
};
} // namespace moexwatch::ui
#endif
