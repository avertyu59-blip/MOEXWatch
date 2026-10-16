#pragma once
#ifdef _WIN32
#include <windows.h>
#include "table_sort_win.hpp"
#include <vector>
#include <string>
#include <atomic>
#include "market_core.hpp"
#include "async_tasks.hpp"

namespace moexwatch::ui {
struct BacktestState {
    HWND ticker{};
    HWND capital{};
    HWND fee{};
    HWND run{};
    HWND result{};
    std::atomic<bool> busy{false};
    std::vector<HWND> controls;
};
using BacktestUiResult=BacktestResult;
}
#endif
