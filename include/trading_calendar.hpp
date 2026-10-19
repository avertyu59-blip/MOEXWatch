#pragma once
#include "market_types.hpp"
#include <string_view>
namespace moexwatch {
struct CalendarCoverage {
    std::string_view first_date, last_date, source;
};
CalendarCoverage trading_calendar_coverage();
// No extrapolation outside explicitly reviewed coverage. An empty calendar
// makes dividend_last_buy fail closed instead of guessing weekdays.
const std::vector<TradingDay> &trading_calendar_for(std::string_view record_date);
} // namespace moexwatch
