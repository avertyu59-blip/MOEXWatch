#include "portfolio_overview.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace moexwatch;
static void check(bool value) { if (!value) throw std::runtime_error("portfolio overview check failed"); }
static void near(double a, double b) { check(std::abs(a - b) < 1e-8); }
int main() {
    PortfolioSummary summary;
    auto empty = calculate_portfolio_overview(summary, {}, 100);
    check(empty && empty->complete && empty->all_fresh); near(*empty->total_assets, 100); check(!empty->top3_percent);
    PortfolioPosition a; a.ticker = "A"; a.quantity = 10; a.average_entry_price = 101; a.market_value = 1200;
    PortfolioPosition b = a; b.ticker = "B"; b.market_value.reset();
    summary.positions = {a, b}; summary.total_realized_profit = -50;
    auto partial = calculate_portfolio_overview(summary, {{"A", QuoteQuality::Fresh}}, 100);
    check(partial && !partial->complete && partial->missing_count == 1 && partial->fresh_count == 1);
    check(!partial->total_assets && !partial->unrealized_profit && !partial->top3_percent);
    near(partial->remaining_cost, 2020); near(partial->priced_value, 1200); near(partial->realized_profit, -50);
    summary.positions[1].market_value = 1300;
    auto stale = calculate_portfolio_overview(summary, {{"A", QuoteQuality::Fresh}, {"B", QuoteQuality::Stale}}, 100);
    check(stale && stale->complete && !stale->all_fresh && stale->stale_count == 1 && !stale->top3_percent);
    near(*stale->total_assets, 2600); near(*stale->unrealized_profit, 480); near(stale->stale_value, 1300);
    auto fresh = calculate_portfolio_overview(summary, {{"A", QuoteQuality::Fresh}, {"B", QuoteQuality::Fresh}}, 10000);
    check(fresh && fresh->all_fresh); near(*fresh->top3_percent, 100); // Cash is not in concentration denominator.
    PortfolioSummary four;
    std::vector<PositionQuoteQuality> four_quality;
    for (int i = 0; i < 4; ++i) {
        auto position = a; position.ticker = std::string(1, static_cast<char>('A' + i)); position.market_value = 100;
        four.positions.push_back(position); four_quality.push_back({position.ticker, QuoteQuality::Fresh});
    }
    near(*calculate_portfolio_overview(four, four_quality, 1000)->top3_percent, 75);
    // Multiple entries in one ticker are aggregated before selecting the top three.
    four.positions.push_back(four.positions.front());
    near(*calculate_portfolio_overview(four, four_quality, 1000)->top3_percent, 80);
    check(!calculate_portfolio_overview(summary, {}, -1));
    check(!calculate_portfolio_overview(summary, {}, std::numeric_limits<double>::infinity()));
    check(!calculate_portfolio_overview(summary, {{"A", QuoteQuality::Fresh}, {"A", QuoteQuality::Stale}}, 0));
    summary.positions[0].market_value = std::numeric_limits<double>::quiet_NaN();
    check(!calculate_portfolio_overview(summary, {}, 0));
    summary.positions[0] = a; summary.positions[0].average_entry_price = std::numeric_limits<double>::max();
    check(!calculate_portfolio_overview(summary, {}, 0));
    summary.positions = {a}; summary.positions[0].market_value = std::numeric_limits<double>::max();
    check(!calculate_portfolio_overview(summary, {{"A", QuoteQuality::Fresh}}, std::numeric_limits<double>::max()));
    const auto income=calculate_portfolio_income(100.0,50.0,20.0);check(income&&income->realized_with_dividends==120.0&&income->total_profit==170.0);
    const auto partialIncome=calculate_portfolio_income(-10.0,{},20.0);check(partialIncome&&partialIncome->realized_with_dividends==10.0&&!partialIncome->total_profit);
    check(!calculate_portfolio_income(0,{},-1));check(!calculate_portfolio_income(std::numeric_limits<double>::max(),{},std::numeric_limits<double>::max()));
    return 0;
}
