#pragma once
#ifdef _WIN32
#include <windows.h>
#include "table_sort_win.hpp"
#include <vector>
#include <string>
#include "market_core.hpp"

namespace moexwatch::ui {
struct TradesState {
    TableSort sort{-1,true};
    bool updating{};
    HWND list{};
    HWND summary{};
    HWND ticker{};
    HWND quantity{};
    HWND buy{};
    HWND sell{};
    HWND commission{};
    HWND tax{};
    std::vector<HWND> controls;
    std::vector<Trade> records;
};
}
#endif
