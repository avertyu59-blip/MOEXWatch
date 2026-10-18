#pragma once
#include "dividend_calculations.hpp"
#include <map>
namespace moexwatch {
std::string dividend_event_key(std::string_view ticker,
                               const DividendEvent &event);
// Allocate actual net income using the frozen, audited entitlement quantities.
std::optional<std::map<std::string, double>> allocate_dividend_income(
    const std::vector<DividendPositionEntitlement> &positions, double gross,
    double tax);
} // namespace moexwatch
