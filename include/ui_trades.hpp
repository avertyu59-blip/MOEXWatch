#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vector>
#include <string>
#include <atomic>
#include "market_core.hpp"
#include "observation.hpp"
#include <shellapi.h>

namespace moexwatch::ui {
struct TradesState {
    HWND list{};
    HWND summary{};
    HWND ticker{};
    HWND quantity{};
    HWND buy{};
    HWND sell{};
    HWND commission{};
    HWND tax{};
    bool building_trade_page = false;
    std::vector<HWND> controls;
    std::vector<Trade> records;
};
}
#endif
