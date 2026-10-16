#pragma once
#ifdef _WIN32
#include <windows.h>
#include <string>
#include <cstdint>
#include "ui_chart_types.hpp"
#include "table_sort_win.hpp"
namespace moexwatch::ui {
struct DividendsState {
    HWND window{};
    HWND list{};
    HWND info{};
    HWND refresh{};
    HWND chart_tabs{};
    HWND chart_range{};
    std::string ticker;
    std::uint64_t request{};
    ChartPanel chart_panel{ChartPanel::Price};
    int chart_sessions{66};
    TableSort sort{-1,true};
    bool updating{};
    bool pending_refresh{};
};
}
#endif
