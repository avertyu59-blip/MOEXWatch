#include "portfolio_identity.hpp"
#include "portfolio_calculations.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
namespace moexwatch {
std::string new_portfolio_id() {
    std::random_device entropy;
    std::array<unsigned char,16> bytes{};
    for(auto& byte:bytes)byte=static_cast<unsigned char>(entropy()&255U);
    bytes[6]=static_cast<unsigned char>((bytes[6]&15U)|64U);bytes[8]=static_cast<unsigned char>((bytes[8]&63U)|128U);
    constexpr char hex[]="0123456789abcdef";
    std::string id;
    for(std::size_t i=0;i<bytes.size();++i){if(i==4||i==6||i==8||i==10)id+='-';id+=hex[bytes[i]>>4];id+=hex[bytes[i]&15];}
    return id;
}
std::vector<PortfolioTransaction> identify_portfolio(std::vector<PortfolioTransaction> values,std::uint64_t sequence_floor) {
    std::set<std::string> ids;
    std::uint64_t next=sequence_floor;
    for(auto& t:values) {
        for(auto& c:t.ticker)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if(!valid_portfolio_transaction(t))throw std::runtime_error("Некорректная операция портфеля");
        if(!t.id.empty()&&(!valid_uuid(t.id)||!ids.insert(t.id).second))throw std::runtime_error("Некорректный или повторный ID операции");
        if(!t.position_id.empty()&&!valid_uuid(t.position_id))throw std::runtime_error("Некорректный ID позиции");
        next=std::max(next,t.sequence);
    }
    for(const auto& t:values)if(!t.position_id.empty())ids.insert(t.position_id);
    const auto unique=[&](){std::string id;do{id=new_portfolio_id();}while(!ids.insert(id).second);return id;};
    for(auto& t:values) {
        if(t.id.empty())t.id=unique();
        if(!t.sequence){if(next==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("Исчерпан порядок операций");t.sequence=++next;}
    }
    std::stable_sort(values.begin(),values.end(),transaction_before);
    struct Active {std::string id;double quantity{};};
    std::map<std::string,Active> active;
    for(auto& t:values) {
        auto ticker=t.ticker;for(auto& c:ticker)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        auto& cycle=active[ticker];
        if(cycle.quantity==0.0) {
            if(t.side!=PortfolioSide::Buy)throw std::runtime_error("Продажа без остатка");
            cycle.id=t.position_id.empty()?unique():t.position_id;
        }
        if(t.position_id.empty())t.position_id=cycle.id;
        if(t.position_id!=cycle.id)throw std::runtime_error("Изменение объединяет разные позиции. Удаление или импорт отменён: ID не переназначаются.");
        cycle.quantity+=t.side==PortfolioSide::Buy?t.quantity:-t.quantity;
        if(t.side==PortfolioSide::Sell&&cycle.quantity<=1e-9)cycle.quantity=0;
    }
    if(!position_cycles(values)||!calculate_portfolio(values,{}))throw std::runtime_error("Нарушены остаток, порядок, уникальность ID или границы циклов позиции");
    return values;
}
PortfolioJournal migrate_portfolio_identity(const std::vector<PortfolioTransaction>& values,const std::vector<PositionLevels>& levels) {
    PortfolioJournal journal;journal.transactions=identify_portfolio(values);
    for(const auto& t:journal.transactions)journal.sequence_watermark=std::max(journal.sequence_watermark,t.sequence);
    const auto cycles=position_cycles(journal.transactions);
    for(auto level:levels) {
        for(auto& c:level.ticker)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if(!valid_position_levels(level))throw std::runtime_error("Некорректные старые уровни");
        std::vector<const PositionCycle*> matches;
        for(const auto& c:*cycles)if(c.ticker==level.ticker&&c.opened_on==level.opened_on)matches.push_back(&c);
        if(matches.size()==1){level.position_id=matches.front()->position_id;journal.levels.push_back(std::move(level));}
        else journal.unresolved_levels.push_back(std::move(level));
    }
    return journal;
}
bool valid_portfolio_journal(const PortfolioJournal& journal) {
    if(!position_cycles(journal.transactions)||!calculate_portfolio(journal.transactions,{}))return false;
    std::set<std::string> ids;
    for(const auto& level:journal.levels)if(!valid_position_levels(level)||!valid_uuid(level.position_id)||!ids.insert(level.position_id).second)return false;
    for(const auto& level:journal.unresolved_levels)if(!valid_position_levels(level)||!level.position_id.empty())return false;
    const auto cycles=position_cycles(journal.transactions);
    for(const auto& level:journal.levels)for(const auto& cycle:*cycles)if(cycle.position_id==level.position_id&&cycle.ticker!=level.ticker)return false;
    return true;
}
}
