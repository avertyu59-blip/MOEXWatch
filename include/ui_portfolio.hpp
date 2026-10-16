#pragma once
#ifdef _WIN32
#include <windows.h>
#include "table_sort_win.hpp"
#include <vector>
#include <string>
#include "market_core.hpp"

namespace moexwatch::ui {
struct PortfolioState {
    TableSort positions_sort{-1,true};
    TableSort transactions_sort{-1,true};
    bool updating{};
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
    std::vector<PositionLevels> unresolved_levels;
    std::uint64_t sequence_watermark{};
    std::vector<std::string> operation_row_ids;
    std::vector<std::string> position_row_ids;
};
}
#endif
