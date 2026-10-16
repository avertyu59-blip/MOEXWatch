#include "position_cycles.hpp"
#include "portfolio_calculations.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
namespace moexwatch {
bool valid_uuid(std::string_view value) noexcept {
    if(value.size()!=36 || value[14]!='4' || (value[19]!='8'&&value[19]!='9'&&value[19]!='a'&&value[19]!='b'))return false;
    for(std::size_t i=0;i<value.size();++i) {
        if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}
        else if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
    }
    return true;
}
bool transaction_before(const PortfolioTransaction& a,const PortfolioTransaction& b) noexcept {
    if(a.date!=b.date)return a.date<b.date;
    // All-legacy callers retain their original stable order.
    return a.sequence<b.sequence;
}
std::optional<std::vector<PositionCycle>> position_cycles(const std::vector<PortfolioTransaction>& input) {
    auto ordered=input;
    std::set<std::string> operations,positions;
    std::set<std::uint64_t> sequences;
    for(const auto& t:ordered) {
        if(!valid_portfolio_transaction(t)||!valid_uuid(t.id)||!valid_uuid(t.position_id)||!t.sequence ||
           !operations.insert(t.id).second||!sequences.insert(t.sequence).second)return {};
    }
    std::stable_sort(ordered.begin(),ordered.end(),transaction_before);
    std::map<std::string,std::size_t> active;
    std::vector<PositionCycle> cycles;
    for(const auto& t:ordered) {
        auto ticker=t.ticker;for(auto& c:ticker)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        auto found=active.find(ticker);
        if(found==active.end()) {
            if(t.side!=PortfolioSide::Buy||!positions.insert(t.position_id).second)return {};
            cycles.push_back({t.position_id,ticker,t.date,"",0});
            found=active.emplace(ticker,cycles.size()-1).first;
        }
        auto& cycle=cycles[found->second];
        if(cycle.position_id!=t.position_id)return {};
        if(t.side==PortfolioSide::Buy)cycle.quantity+=t.quantity;
        else {
            if(t.quantity>cycle.quantity+1e-9)return {};
            cycle.quantity-=t.quantity;
            if(cycle.quantity<=1e-9){cycle.quantity=0;cycle.closed_on=t.date;active.erase(found);}
        }
        if(!std::isfinite(cycle.quantity))return {};
    }
    return cycles;
}
}
