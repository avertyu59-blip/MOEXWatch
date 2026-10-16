#pragma once
#include "market_types.hpp"
namespace moexwatch {
std::optional<std::string> dividend_last_buy(std::string_view record_date,
    const std::vector<TradingDay>& calendar);
enum class DividendYieldBasis { Unavailable, CurrentPrice, LastBuyClose };
struct DividendYield {
    std::optional<double> percent;
    DividendYieldBasis basis{DividendYieldBasis::Unavailable};
};
DividendYield calculate_dividend_yield(const DividendEvent& event, std::string_view as_of,
    std::optional<double> current_price, bool quote_stale,
    const std::vector<Candle>& candles, bool candles_stale);
enum class DividendEntitlementStatus { Available, Unconfirmed, InsufficientData, InvalidOperations };
struct DividendEntitlement {
    DividendEntitlementStatus status{DividendEntitlementStatus::InsufficientData};
    std::optional<double> quantity;
    std::optional<double> gross_amount;
    bool projected{};
    std::string reason;
};
// Expected gross entitlement from a complete long-only journal, not a payment confirmation.
// Future cutoff: only operations through as_of are counted; projected is true.
// Caller must mark stale/conflicting source data unavailable before relying on this estimate.
DividendEntitlement calculate_dividend_entitlement(std::string_view ticker,
    const DividendEvent& event, const std::vector<PortfolioTransaction>& transactions,
    const std::vector<TradingDay>& calendar, std::string_view as_of);
}
