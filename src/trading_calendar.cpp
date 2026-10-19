#include "trading_calendar.hpp"
#include "market_core.hpp"
#include <charconv>
#include <chrono>
namespace moexwatch {
CalendarCoverage trading_calendar_coverage() {
    return {"2026-01-01", "2026-12-31", "https://www.moex.com/n96571"};
}
const std::vector<TradingDay> &trading_calendar_for(std::string_view date) {
    static const auto supported = moex_calendar_2026();
    static const std::vector<TradingDay> empty;
    const auto coverage = trading_calendar_coverage();
    if (date.size() != 10 || date[4] != '-' || date[7] != '-' || date < coverage.first_date ||
        date > coverage.last_date)
        return empty;
    int y = 0, m = 0, d = 0;
    const auto number = [](std::string_view v, int &n) {
        const auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
        return ec == std::errc{} && p == v.data() + v.size();
    };
    if (!number(date.substr(0, 4), y) || !number(date.substr(5, 2), m) || !number(date.substr(8, 2), d) ||
        !std::chrono::year_month_day(std::chrono::year(y), std::chrono::month(static_cast<unsigned>(m)),
                                     std::chrono::day(static_cast<unsigned>(d)))
             .ok())
        return empty;
    return supported;
}
} // namespace moexwatch
