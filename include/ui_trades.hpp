#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vector>
#include <string>
#include "market_core.hpp"

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
    std::vector<HWND> controls;
    std::vector<Trade> records;
};
}
#endif
