#pragma once
#ifdef _WIN32
#include <windows.h>
#include "portfolio_csv.hpp"
#include <functional>
namespace moexwatch {
bool review_portfolio_import(HWND parent, HFONT font, std::string csv,
    const std::vector<PortfolioTransaction>& existing,
    std::function<void(const std::vector<PortfolioTransaction>&)> commit);
}
#endif
