#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vector>
#include <string>
#include "market_core.hpp"

namespace moexwatch::ui {
enum class ChartPanel { Price, Comparison, Volume, Rsi };
struct ChartsState {
    HWND list{};
    HWND ticker_edit{};
    HWND target_edit{};
    HWND status{};
    HWND analysis{};
    HWND dividend_window{};
    HWND dividend_list{};
    HWND dividend_info{};
    HWND dividend_refresh{};
    HWND chart_tabs{};
    HWND dividend_chart_tabs{};
    HWND dividend_chart_range{};
    std::size_t selected{};
    std::size_t dividend_instrument{};
    unsigned int dividend_request{};
    std::vector<std::size_t> visible;
    std::vector<HWND> market_controls;
    WatchFilter filter = WatchFilter::All;
    int chart_sessions = 66;
    ChartPanel chart_panel = ChartPanel::Price;
    ChartPanel dividend_chart_panel = ChartPanel::Price;
    int dividend_chart_sessions = 66;
};
}
#endif
