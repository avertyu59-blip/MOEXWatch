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
struct BacktestState {
    HWND ticker{};
    HWND capital{};
    HWND fee{};
    HWND run{};
    HWND result{};
    bool building_backtest_page = false;
    std::atomic<bool> busy{false};
    std::vector<HWND> controls;
};
struct BacktestUiResult { std::string text; bool failed{}; };
}
#endif
