#include "portfolio_calculations.hpp"
#include "indicators.hpp"
#include "signal_rules.hpp"
#include "backtest.hpp"
#include "dividend_calculations.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace moexwatch;
namespace {
void check(bool value) { if (!value) throw std::runtime_error("domain check failed"); }
void near(double a,double b) { check(std::abs(a-b)<1e-8); }
}
int main() {
    const std::vector<Trade> trades{{"SBER",10,100,120,10,13},{"SBER",5,120,100,5,13}};
    const auto total=calculate_trade_totals(trades); check(total.has_value());
    near(total->gross_profit,100); near(total->profit_after_commission,85);
    near(total->tax,24.7); near(total->net_profit,60.3); // Existing per-trade tax estimate, not annual offset.
    check(!calculate_trade_totals({{"SBER",1,0,100,0,13}}));
    check(calculate_trade_totals({})->net_profit==0);
    std::vector<PortfolioTransaction> operations{{"2026-07-01","SBER",PortfolioSide::Buy,10,100,10},
        {"2026-07-02","SBER",PortfolioSide::Buy,10,200,10},
        {"2026-07-06","SBER",PortfolioSide::Sell,5,180,5}};
    auto portfolio=calculate_portfolio(operations,{{"SBER",180,{},"", ""}}); check(portfolio.has_value());
    near(portfolio->positions[0].average_entry_price,151); near(portfolio->total_realized_profit,140);
    near(portfolio->total_unrealized_profit,435); near(*portfolio_weight(*portfolio,portfolio->positions[0],true),100);
    check(!portfolio_weight(*portfolio,portfolio->positions[0],false));
    check(!calculate_portfolio(operations,{})->positions[0].unrealized_profit);
    auto invalid=operations;invalid.push_back({"2026-07-07","SBER",PortfolioSide::Sell,100,180,0});
    check(!calculate_portfolio(invalid,{}));
    std::vector<Candle> flat(220,{"2026-01-01",100,102,98,100,1000});
    near(*analyze(flat).rsi14,50);near(*analyze(flat).atr14,4);near(*analyze(flat).sma200,100);
    near(*analyze(flat).volume_ratio20,1); check(!analyze({}).rsi14);
    check(!sma_series(flat,0).back()); check(!rsi_series(std::vector<Candle>(14),14).back());
    auto malformed = flat; malformed[5].close=std::numeric_limits<double>::quiet_NaN();
    check(!analyze(malformed).rsi14);check(!sma_series(malformed,20).back());
    malformed=flat;malformed[2].volume=-1;check(!volume_ratio_series(malformed).back());
    malformed=flat;malformed[2].low=103;check(!atr_series(malformed).back());
    SignalSettings settings; settings.above_sma200_enabled=false;settings.near_target_enabled=false;
    Analysis analysis;analysis.rsi14=20;
    check(evaluate_signals(analysis,100,{},false,false,settings).matched);
    check(!evaluate_signals(analysis,100,{},true,false,settings).matched);
    check(!evaluate_signals({},100,{},false,false,settings).matched);
    settings.require_all=true;settings.above_sma200_enabled=true;
    check(!evaluate_signals(analysis,100,{},false,false,settings).matched);
    DividendEvent event; event.record_date="2026-07-08";event.amount_per_share=10;
    event.currency="RUB";event.status=DividendStatus::Approved;
    std::vector<TradingDay> calendar{{"2026-07-01",true,""},{"2026-07-02",true,""},
        {"2026-07-03",true,""},{"2026-07-04",false,""},{"2026-07-05",false,""},
        {"2026-07-06",true,""},{"2026-07-07",true,""},{"2026-07-08",true,""}};
    check(dividend_last_buy(event.record_date,calendar)=="2026-07-07");
    auto entitlement=calculate_dividend_entitlement("SBER",event,operations,calendar,"2026-07-09");
    check(entitlement.status==DividendEntitlementStatus::Available); near(*entitlement.quantity,15);near(*entitlement.gross_amount,150);check(!entitlement.projected);
    auto later=operations;later.push_back({"2026-07-08","SBER",PortfolioSide::Sell,15,180,0});
    near(*calculate_dividend_entitlement("SBER",event,later,calendar,"2026-07-09").quantity,15);
    later.push_back({"2026-07-08","SBER",PortfolioSide::Buy,2,180,0});
    near(*calculate_dividend_entitlement("SBER",event,later,calendar,"2026-07-09").quantity,15);
    auto projected=calculate_dividend_entitlement("SBER",event,operations,calendar,"2026-07-02");
    check(projected.projected);near(*projected.quantity,20);
    near(*calculate_dividend_entitlement("SBER",event,{},calendar,"2026-07-09").quantity,0);
    check(calculate_dividend_entitlement("SBER",event,invalid,calendar,"2026-07-09").status==DividendEntitlementStatus::InvalidOperations);
    check(!calculate_dividend_entitlement("SBER",event,operations,{},"2026-07-09").quantity);
    event.status=DividendStatus::Forecast;
    check(calculate_dividend_entitlement("SBER",event,operations,calendar,"2026-07-09").status==DividendEntitlementStatus::Unconfirmed);
    event.status=DividendStatus::Approved; event.last_buy_date="2026-07-06";
    check(!calculate_dividend_entitlement("SBER",event,operations,calendar,"2026-07-09").quantity);
    event.last_buy_date="2026-07-07";
    auto yield=calculate_dividend_yield(event,"2026-07-01",200,false,{},false);
    near(*yield.percent,5);check(yield.basis==DividendYieldBasis::CurrentPrice);
    check(!calculate_dividend_yield(event,"2026-07-01",200,true,{},false).percent);
    const std::vector<Candle> closing{{"2026-07-07",100,100,100,100,1000}};
    near(*calculate_dividend_yield(event,"2026-07-09",200,false,closing,false).percent,10);
    check(!calculate_dividend_yield(event,"2026-07-09",200,false,closing,true).percent);
    event.verification=DividendVerification::Conflict;
    check(!calculate_dividend_yield(event,"2026-07-01",200,false,{},false).percent);
    check(!calculate_dividend_entitlement("SBER",event,operations,calendar,"2026-07-09").quantity);
    std::cout<<"domain_tests: all checks passed\n";
}
