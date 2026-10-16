#include "market_data.hpp"
#include "storage.hpp"
#include "market_validation.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
namespace moexwatch {
namespace {
std::wstring widen(std::string_view text) {
    if (text.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}

}
std::string https_get(FetchContext& context,const std::wstring& path,const wchar_t* host=L"iss.moex.com") {
    check_cancelled(context.stop);
    const auto ascii=[](std::wstring_view value){std::string text;text.reserve(value.size());for(const wchar_t c:value){if(c<0||c>127)throw ValidationFailure("Ожидался ASCII-адрес источника");text+=static_cast<char>(c);}return text;};
    const auto hostname=ascii(host),endpoint=ascii(path);
    const std::string source=hostname=="iss.moex.com"?"moex":hostname=="smart-lab.ru"?"smartlab":"issuer";
    return get_with_retry({hostname,endpoint,source},context.transport,context.stop,context.log.get(),context.request).body;
}
void checked_stage(FetchContext& context,std::string_view source,bool accepted) {
    check_cancelled(context.stop);
    if(context.log)context.log->record(context.request,source,accepted?"validation_ok":"validation_failed");
}

std::string date_from() {
    const auto day = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) - std::chrono::days{730};
    const std::chrono::year_month_day ymd{day};
    std::ostringstream out;
    out << static_cast<int>(ymd.year()) << '-' << std::setfill('0') << std::setw(2) << static_cast<unsigned>(ymd.month())
        << '-' << std::setw(2) << static_cast<unsigned>(ymd.day());
    return out.str();
}

std::string local_timestamp() {
    SYSTEMTIME time{}; GetLocalTime(&time); std::ostringstream out;
    out << time.wYear << '-' << std::setfill('0') << std::setw(2) << time.wMonth << '-'
        << std::setw(2) << time.wDay << ' ' << std::setw(2) << time.wHour << ':' << std::setw(2) << time.wMinute;
    return out.str();
}

std::filesystem::path dividends_cache_path(std::string_view ticker) {
    if (!valid_ticker(ticker)) throw std::runtime_error("Некорректный тикер кэша");
    return storage::data_directory() / (L"dividends_" + widen(ticker) + L".csv");
}

void load_dividend_cache(Instrument& item) {
    if (!item.dividends_checked_at.empty()) return;
    const auto bytes = storage::read_text(dividends_cache_path(item.ticker), 1024U*1024U);
    if (!bytes) return;
    const auto& csv = *bytes;
    const auto saved = parse_dividend_snapshot(csv, item.ticker);
    if (!saved) { item.dividend_note = "Сохранённый файл повреждён; ожидается загрузка из сети."; return; }
    item.dividends = saved->events; item.dividends_checked_at = saved->checked_at;
    item.dividends_stale = true;
}

void save_dividend_cache(const Instrument& item) {
    storage::atomic_write(dividends_cache_path(item.ticker),
        serialize_dividend_snapshot({item.ticker,item.dividends_checked_at,item.dividends}));
}

void fetch_dividend_events(DividendFetchResult& result,FetchContext& context) {
    const auto ticker = result.ticker;
    const std::string history_url = "https://smart-lab.ru/q/" + ticker + "/dividend/";
    std::vector<DividendEvent> history, upcoming;
    // Either page failing is a partial refresh, never a successful empty calendar.
    // Keep the last complete snapshot instead of erasing cancelled/withdrawn events.
    history = parse_smartlab_dividends(https_get(context,L"/q/" + widen(ticker) + L"/dividend/", L"smart-lab.ru"), ticker, history_url);
    upcoming = parse_smartlab_dividends(https_get(context,L"/dividends/", L"smart-lab.ru"), ticker, "https://smart-lab.ru/dividends/");
    merge_dividends(history, upcoming);
    if (ticker == "LKOH") {
        const std::string issuer = "https://lukoil.ru/InvestorAndShareholderCenter/Securities/Dividends";
        try {
            const auto official = parse_lukoil_dividends(https_get(context,L"/InvestorAndShareholderCenter/Securities/Dividends", L"lukoil.ru"));
            reconcile_dividends(history, official, issuer, result.checked_at);
            result.note = "Сверка: официальная таблица ЛУКОЙЛ (совпадение даты и суммы).";
        } catch (const Cancelled&) {throw;} catch (const std::exception&) {
            result.note = "Сайт эмитента недоступен либо изменён; сверка не выполнена.";
            for (auto& e : history) { e.issuer_url = issuer; e.issuer_checked_at = result.checked_at; e.verification = DividendVerification::Unavailable; }
        }
    } else result.note = "Автоматическая сверка с эмитентом для этой бумаги пока не подключена.";
    const auto calendar = moex_calendar_2026();
    for (auto& e : history) {
        const auto buy = dividend_last_buy(e.record_date, calendar);
        if (buy) {
            e.last_buy_date = *buy;
            e.last_buy_basis = "Расчёт T+1, MOEX 2026: https://www.moex.com/n96571 (сверено 15.10.2026)";
            if (!e.reported_last_buy.empty() && e.reported_last_buy != *buy)
                e.last_buy_basis += "; отличается от даты календаря источника";
        }
    }
    checked_stage(context,"dividends",true);
    result.events = std::move(history);
}

namespace {
std::vector<Candle> fetch_candle_history(const std::wstring& base, std::string_view ticker,FetchContext& context) {
    std::vector<Candle> saved;
    try { if(const auto cache=storage::load_candles(context.cache_root,ticker)) saved=cache->candles; }
    catch(const std::exception&) { /* A broken cache is disposable; a complete download replaces it. */ }
    const auto window=date_from();
    std::string from=window;
    if(saved.size()>=2) from=std::max(window,saved[saved.size()-2].date.substr(0,10));
    std::vector<Candle> incoming;
    bool complete=false;
    for(std::size_t page=0;page<16;++page) {
        const auto path=base+L"/candles.csv?from="+widen(from)+
            L"&interval=24&iss.meta=on&iss.only=candles&candles.columns=begin,open,high,low,close,volume&iss.delimiter=%3B&iss.dp=point&lang=en&start="+std::to_wstring(incoming.size());
        const auto csv=https_get(context,path);
        std::vector<Candle> batch;
        try{batch=validate_candle_page(csv);checked_stage(context,"candles",true);}
        catch(const Cancelled&){throw;}catch(...){checked_stage(context,"candles",false);throw;}
        if(batch.empty()){complete=true;break;}
        if(!incoming.empty() && batch.front().date.substr(0,10)<=incoming.back().date.substr(0,10))throw std::runtime_error("MOEX повторил страницу истории");
        incoming.insert(incoming.end(),batch.begin(),batch.end());
    }
    if(!complete)throw std::runtime_error("История превышает лимит страниц");
    // An empty incremental response cannot prove freshness of a cached tail.
    if(incoming.empty())throw std::runtime_error("MOEX не вернул дневные свечи; сохранённая история не подтверждена");
    auto result=storage::merge_candles(saved,incoming,window);
    if(result.empty())throw std::runtime_error("MOEX не вернул историю за выбранный период");
    check_cancelled(context.stop);
    storage::save_candles(context.cache_root,{std::string(ticker),local_timestamp(),result});
    return result;
}
}

void load_market_cache(std::vector<Instrument>& items,std::vector<Candle>& index,std::string& index_error) {
    for(auto& item:items)try {
        if(const auto cache=storage::load_candles(storage::data_directory(),item.ticker)) {
            item.candles=storage::merge_candles(cache->candles,{},date_from());
            item.candles_checked_at=cache->checked_at;item.candles_stale=true;
        }
    } catch(const std::exception& ex){item.error=std::string("Кэш свечей: ")+ex.what();}
    try {if(const auto cache=storage::load_candles(storage::data_directory(),"IMOEX"))index=storage::merge_candles(cache->candles,{},date_from());}
    catch(const std::exception& ex){index_error=std::string("Кэш IMOEX: ")+ex.what();}
}

Instrument fetch_instrument(const Instrument& previous,FetchContext& context) {
    Instrument item=previous;item.error.clear();item.quote_stale=true;item.candles_stale=true;
    if(item.candles.empty())try {
        if(const auto cache=storage::load_candles(context.cache_root,item.ticker)) {
            item.candles=storage::merge_candles(cache->candles,{},date_from());item.candles_checked_at=cache->checked_at;
        }
    } catch(const std::exception&) {}
    const std::wstring base=L"/iss/engines/stock/markets/shares/boards/TQBR/securities/"+widen(item.ticker);
    try {
        const auto csv=https_get(context,base+L".csv?iss.meta=on&iss.only=marketdata&marketdata.columns=SECID,LAST,LASTCHANGEPRCNT,UPDATETIME,SYSTIME&iss.delimiter=%3B&iss.dp=point&lang=en");
        const auto quote=validate_quote_response(csv,item.ticker);checked_stage(context,"quote",true);
        item.quote=quote;item.quote_stale=!quote.last.has_value();
        if(!quote.last)item.error="MOEX не вернул цену последней сделки";
    } catch(const Cancelled&){throw;}catch(const std::exception& ex){checked_stage(context,"quote",false);item.error=std::string("Котировки: ")+ex.what();}
    try {item.candles=fetch_candle_history(base,item.ticker,context);item.candles_stale=false;item.candles_checked_at=local_timestamp();}
    catch(const Cancelled&){throw;}catch(const std::exception& ex){if(!item.error.empty())item.error+="; ";item.error+=std::string("График: ")+ex.what();}
    check_cancelled(context.stop);
    return item;
}

std::vector<Candle> fetch_index_candles(FetchContext& context) {
    return fetch_candle_history(L"/iss/engines/stock/markets/index/boards/SNDX/securities/IMOEX","IMOEX",context);
}
}
#endif
