#pragma once
#ifdef _WIN32
#include <windows.h>
#include <optional>
#include <cstddef>
namespace moexwatch {
struct TableSort { int column{-1}; bool ascending{true}; };
std::optional<std::size_t> selected_row_data(HWND list);
std::optional<std::size_t> row_data(HWND list,int row);
void sort_report(HWND list,const TableSort& sort,bool numeric);
}
#endif
