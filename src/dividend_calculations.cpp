#include "dividend_calculations.hpp"
#include "portfolio_calculations.hpp"
#include "domain_common.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <map>
#include <iomanip>
#include <sstream>
namespace moexwatch {
namespace {
std::optional<std::chrono::sys_days> day(std::string_view s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return {};
    for (std::size_t i = 0; i < s.size(); ++i) if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return {};
    const int y = std::stoi(std::string(s.substr(0, 4)));
    const unsigned m = static_cast<unsigned>(std::stoi(std::string(s.substr(5, 2))));
    const unsigned d = static_cast<unsigned>(std::stoi(std::string(s.substr(8, 2))));
    const std::chrono::year_month_day date{std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}};
    if (y < 1990 || y > 2100 || !date.ok()) return {};
    return std::chrono::sys_days{date};
}
std::string iso(std::chrono::sys_days d) {
    const std::chrono::year_month_day ymd{d}; std::ostringstream s;
    s << int(ymd.year()) << '-' << std::setfill('0') << std::setw(2) << unsigned(ymd.month())
      << '-' << std::setw(2) << unsigned(ymd.day()); return s.str();
}
}
std::optional<std::string> dividend_last_buy(std::string_view record,const std::vector<TradingDay>& days) {
    const auto rd=day(record);if(!rd)return {};
    std::map<std::string,TradingDay> calendar;
    for(const auto& d:days) {if(!day(d.date))return {};if(!calendar.emplace(d.date,d).second)return {};}
    // No weekday guesses: require explicit coverage up to the record date.
    if (!calendar.contains(std::string(record))) return {};
    auto start = *rd;
    for (int i=0;i<21;++i) {
        const auto previous=start-std::chrono::days{1};
        if (!calendar.contains(iso(previous))) break;
        start=previous;
    }

    const int lag=record<"2023-07-31" ? 2:1;
    std::vector<std::string> business;
    for(const auto& [key,d]:calendar)if(d.traded && (d.session_date.empty()||d.session_date==key))business.push_back(key);
    std::optional<std::string> last;
    for(const auto& [key,d]:calendar) {
        if(!d.traded || key>record || key<iso(start))continue;
        const auto effective=d.session_date.empty()?key:d.session_date;
        const auto it=std::lower_bound(business.begin(),business.end(),effective);
        if(it==business.end() || *it!=effective)continue;
        // Weekend sessions carry the same business date, not an extra settlement day.
        const auto offset=std::distance(it,business.end());
        if(offset<=lag)continue;
        if(*(it+lag)<=record)last=key;
    }
    return last;
}



DividendYield calculate_dividend_yield(const DividendEvent& event, std::string_view as_of,
    std::optional<double> price, bool quote_stale, const std::vector<Candle>& candles, bool candles_stale) {
    DividendYield result;
    if (!day(as_of) || !std::isfinite(event.amount_per_share) || event.amount_per_share <= 0.0 ||
        event.status == DividendStatus::Cancelled || event.verification == DividendVerification::Conflict) return result;
    const bool historical = !event.record_date.empty() && event.record_date < as_of;
    std::optional<double> basis;
    if (!historical && price && std::isfinite(*price) && *price > 0.0 && !quote_stale) {
        basis = price; result.basis = DividendYieldBasis::CurrentPrice;
    } else if (!event.last_buy_date.empty() && !candles_stale) {
        const auto found = std::find_if(candles.begin(), candles.end(), [&](const auto& c) {
            return c.date.substr(0,10) == event.last_buy_date;
        });
        if (found != candles.end() && std::isfinite(found->close) && found->close > 0.0) {
            basis = found->close; result.basis = DividendYieldBasis::LastBuyClose;
        }
    }
    if (basis) {
        const double value = event.amount_per_share / *basis * 100.0;
        if (std::isfinite(value)) result.percent = value;
    }
    if (!result.percent) result.basis = DividendYieldBasis::Unavailable;
    return result;
}

DividendEntitlement calculate_dividend_entitlement(std::string_view ticker, const DividendEvent& event,
    const std::vector<PortfolioTransaction>& transactions, const std::vector<TradingDay>& calendar,
    std::string_view as_of) {
    DividendEntitlement result;
    if (event.status != DividendStatus::Approved || event.verification == DividendVerification::Conflict) {
        result.status = DividendEntitlementStatus::Unconfirmed;
        result.reason = "Нет утверждённой выплаты без расхождений"; return result;
    }
    const auto cutoff = dividend_last_buy(event.record_date, calendar);
    if (!valid_ticker(ticker) || !day(as_of) || !cutoff ||
        (!event.last_buy_date.empty() && event.last_buy_date != *cutoff) ||
        !std::isfinite(event.amount_per_share) || event.amount_per_share <= 0.0 || event.currency != "RUB") {
        result.reason = "Недостаточно согласованных дат, календаря или суммы в RUB"; return result;
    }
    std::string key(ticker);
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    std::vector<PortfolioTransaction> relevant;
    for (const auto& operation : transactions) {
        std::string name = operation.ticker;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (name != key) continue;
        if (!valid_portfolio_transaction(operation)) {
            result.status = DividendEntitlementStatus::InvalidOperations;
            result.reason = "Некорректная операция по бумаге"; return result;
        }
        if (operation.date <= as_of) relevant.push_back(operation);
    }
    if (!calculate_portfolio(relevant, {})) {
        result.status = DividendEntitlementStatus::InvalidOperations;
        result.reason = "Продажа превышает остаток"; return result;
    }
    std::erase_if(relevant, [&](const auto& operation) { return operation.date > *cutoff; });
    const auto portfolio = calculate_portfolio(relevant, {});
    if (!portfolio) {
        result.status = DividendEntitlementStatus::InvalidOperations;
        result.reason = "Несогласованный журнал на последнюю дату покупки"; return result;
    }
    double quantity = 0.0;
    for (const auto& position : portfolio->positions) if (position.ticker == key) quantity = position.quantity;
    const double gross = quantity * event.amount_per_share;
    if (!std::isfinite(gross)) { result.reason = "Сумма выходит за диапазон чисел"; return result; }
    result.status = DividendEntitlementStatus::Available;
    result.quantity = quantity; result.gross_amount = gross; result.projected = as_of < *cutoff;
    result.reason = result.projected ? "Прогноз по журналу на сегодня; состав может измениться" :
        "Ожидается по полному журналу на последнюю дату покупки; до налогов, не факт поступления";
    return result;
}

} // namespace moexwatch
