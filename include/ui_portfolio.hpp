#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vector>
#include <string>
#include "market_core.hpp"

namespace moexwatch::ui {
struct PortfolioState {
    HWND date{};
    HWND ticker{};
    HWND side{};
    HWND quantity{};
    HWND price{};
    HWND commission{};
    HWND add{};
    HWND delete_button{};
    HWND positions{};
    HWND transactions_list{};
    HWND summary{};
    HWND position_stop{};
    HWND position_take{};
    HWND position_levels_save{};
    std::vector<HWND> controls;
    std::vector<PortfolioTransaction> transactions;
    std::vector<PositionLevels> position_levels;
};
}
#endif
