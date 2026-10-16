#include "market_validation.hpp"
#include "market_core.hpp"
#include "io_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <charconv>
namespace moexwatch {
Quote validate_quote_response(std::string_view csv,std::string_view ticker) {
    if(!valid_ticker(ticker))throw ValidationFailure("Некорректный тикер запроса");
    const auto rows=parse_csv(csv);
    const auto header=std::find(rows.begin(),rows.end(),std::vector<std::string>{"SECID","LAST","LASTCHANGEPRCNT","UPDATETIME","SYSTIME"});
    if(header==rows.end()||std::distance(header,rows.end())!=2||std::next(header)->size()!=5||std::next(header)->at(0)!=ticker)
        throw ValidationFailure("MOEX: неполная или неожиданная строка котировки");
    const auto& row=*std::next(header);
    for(std::size_t i:{1U,2U})if(!row[i].empty()){
        double value{};const auto [end,error]=std::from_chars(row[i].data(),row[i].data()+row[i].size(),value);
        if(error!=std::errc{}||end!=row[i].data()+row[i].size()||!std::isfinite(value))throw ValidationFailure("MOEX: некорректное число котировки");
    }
    const auto quote=parse_quote_csv(csv,ticker);
    if(!quote||quote->ticker!=ticker)throw ValidationFailure("MOEX: формат или тикер котировки не совпадает с запросом");
    if(quote->last&&(!std::isfinite(*quote->last)||*quote->last<=0))throw ValidationFailure("MOEX: некорректная цена");
    return *quote;
}
std::vector<Candle> validate_candle_page(std::string_view csv) {
    if(!has_candle_header(csv))throw ValidationFailure("MOEX: формат страницы истории не распознан");
    auto batch=parse_candles_csv(csv);const auto rows=parse_csv(csv);
    const auto header=std::find(rows.begin(),rows.end(),std::vector<std::string>{"begin","open","high","low","close","volume"});
    if(header==rows.end()||static_cast<std::size_t>(std::distance(header,rows.end())-1)!=batch.size())
        throw ValidationFailure("MOEX: история содержит неполные строки");
    for(auto row=std::next(header);row!=rows.end();++row){
        if(row->size()!=6)throw ValidationFailure("MOEX: неполная строка истории");
        for(std::size_t i=1;i<6;++i){double value{};const auto& raw=(*row)[i];const auto [end,error]=std::from_chars(raw.data(),raw.data()+raw.size(),value);
            if(error!=std::errc{}||end!=raw.data()+raw.size()||!std::isfinite(value))throw ValidationFailure("MOEX: некорректное число истории");}
    }
    std::string previous;
    for(const auto& c:batch){
        const auto date=c.date.substr(0,10);
        if(!valid_portfolio_transaction({date,"SBER",PortfolioSide::Buy,1,1,0})||(!previous.empty()&&date<=previous)||
            !std::isfinite(c.open)||!std::isfinite(c.close)||!std::isfinite(c.high)||!std::isfinite(c.low)||!std::isfinite(c.volume)||
            c.open<=0||c.close<=0||c.low<=0||c.volume<0||c.high<std::max(c.open,c.close)||c.low>std::min(c.open,c.close))
            throw ValidationFailure("MOEX: некорректные OHLC, объём или порядок дат");
        previous=date;
    }
    return batch;
}
}
