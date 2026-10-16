#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vector>
#include <string>
#include "market_core.hpp"
#include "app_state.hpp"
#include "table_sort_win.hpp"
#include "ui_chart_types.hpp"

namespace moexwatch::ui {
struct ChartsState {
    InstrumentSelection selection;
    TableSort sort{-1,true};
    bool updating{};
    HWND list{};
    HWND ticker_edit{};
    HWND target_edit{};
    HWND status{};
    HWND analysis{};
    HWND chart_tabs{};
    std::vector<std::size_t> visible;
    std::vector<HWND> market_controls;
    WatchFilter filter = WatchFilter::All;
    int chart_sessions = 66;
    ChartPanel chart_panel = ChartPanel::Price;
};
}
#endif
