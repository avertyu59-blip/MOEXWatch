#include "market_core.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace moexwatch {
namespace {
std::string trim(std::string s) {
    const auto a = s.find_first_not_of(" \r\n\t");
    return a == std::string::npos ? "" : s.substr(a, s.find_last_not_of(" \r\n\t") - a + 1);
}
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
void replace_all(std::string& s, std::string_view a, std::string_view b) {
    std::size_t p = 0;
    while ((p = s.find(a, p)) != std::string::npos) { s.replace(p, a.size(), b); p += b.size(); }
}
std::string text(std::string_view html) {
    std::string s; bool tag = false;
    for (char c : html) {
        if (c == '<') { tag = true; s += ' '; }
        else if (c == '>') { tag = false; s += ' '; }
        else if (!tag) s += c;
    }
    for (const auto a : {"&nbsp;", "&#160;", "&#xA0;", "\xC2\xA0", "\xE2\x80\xAF"}) replace_all(s, a, " ");
    replace_all(s, "&amp;", "&"); replace_all(s, "&quot;", "\"");
    std::string result; bool space = true;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) { if (!space) result += ' '; space = true; }
        else { result += c; space = false; }
    }
    return trim(std::move(result));
}
struct Row { std::string markup; std::vector<std::string> cells; bool header{}; };
std::vector<std::vector<Row>> tables(std::string_view html) {
    if (html.size() > 3U * 1024U * 1024U) throw std::runtime_error("Страница дивидендов слишком велика");
    const auto h = lower(std::string(html));
    std::vector<std::vector<Row>> result;
    std::size_t p = 0;
    while ((p = h.find("<table", p)) != std::string::npos) {
        const auto end = h.find("</table>", p);
        if (end == std::string::npos) throw std::runtime_error("Неполная таблица дивидендов");
        std::vector<Row> rows;
        std::size_t r = p;
        while ((r = h.find("<tr", r)) != std::string::npos && r < end) {
            const auto re = h.find("</tr>", r);
            if (re == std::string::npos || re > end) throw std::runtime_error("Неполная строка дивидендов");
            Row row; row.markup = std::string(html.substr(r, re + 5 - r));
            std::size_t c = r;
            while (c < re) {
                const auto td = h.find("<td", c), th = h.find("<th", c);
                const auto begin = std::min(td, th);
                if (begin == std::string::npos || begin >= re) break;
                const bool header = begin == th;
                const auto content = h.find('>', begin);
                const auto ce = h.find(header ? "</th>" : "</td>", content);
                if (content == std::string::npos || ce == std::string::npos || ce > re)
                    throw std::runtime_error("Неполная ячейка дивидендов");
                row.header = row.header || header;
                row.cells.push_back(text(html.substr(content + 1, ce - content - 1)));
                c = ce + 5;
            }
            if (!row.cells.empty()) rows.push_back(std::move(row));
            r = re + 5;
        }
        result.push_back(std::move(rows)); p = end + 8;
    }
    return result;
}
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
std::string date(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 10 && s[2] == '.' && s[5] == '.') {
        auto d = s.substr(6,4) + '-' + s.substr(3,2) + '-' + s.substr(0,2);
        if (day(d)) return d;
    }
    if (day(s)) return s;
    const std::string months[] = {"января","февраля","марта","апреля","мая","июня","июля","августа","сентября","октября","ноября","декабря"};
    for (unsigned i = 0; i < 12; ++i) {
        const auto p = s.find(months[i]);
        if (p == std::string::npos) continue;
        const auto ds = trim(s.substr(0,p)); const auto ys = trim(s.substr(p + months[i].size()));
        if (ds.empty() || ds.size() > 2 || ys.size() < 4) return {};
        std::string d = ys.substr(0,4) + '-' + (i < 9 ? "0" : "") + std::to_string(i+1) + '-' + (ds.size()==1 ? "0" : "") + ds;
        if (day(d)) return d;
    }
    return {};
}
std::optional<double> amount(std::string s) {
    replace_all(s,"₽",""); replace_all(s,"П",""); replace_all(s," ","");
    std::replace(s.begin(),s.end(),',','.');
    double n{}; const auto [p,e] = std::from_chars(s.data(),s.data()+s.size(),n);
    if (e != std::errc{} || p != s.data()+s.size() || !std::isfinite(n) || n <= 0.0) return {};
    return n;
}
bool contains(std::string_view s, std::string_view q) { return s.find(q) != std::string_view::npos; }
std::string quoted(std::string s) { replace_all(s,"\"","\"\""); return '"' + s + '"'; }
}

std::vector<DividendEvent> parse_smartlab_dividends(std::string_view html, std::string_view ticker, std::string_view url) {
    if (!valid_ticker(ticker)) throw std::runtime_error("Некорректный тикер");
    std::vector<DividendEvent> result; bool recognized = false;
    for (const auto& table : tables(html)) {
        std::optional<std::size_t> ti,di,ai,bi,pi;
        bool history = false;
        for (const auto& row : table) {
            if (row.header) {
                ti.reset();di.reset();ai.reset();bi.reset();pi.reset();
                for (std::size_t c=0;c<row.cells.size();++c) {
                    const auto& v=row.cells[c];
                    if (v=="Тикер") ti=c;
                    else if (contains(v,"отсечки") || contains(v,"закрытия реестра")) di=c;
                    else if (v=="дивиденд" || contains(v,"Дивиденд,")) ai=c;
                    else if (contains(v,"T-1") || contains(v,"Купить До")) bi=c;
                    else if (v=="Период") pi=c;
                }
                history = ti && *ti == 0;
                if (ti && di && ai) recognized = true;
                continue;
            }
            if (!ti || !di || !ai || row.cells.size()<=std::max({*ti,*di,*ai}) || row.cells[*ti]!=ticker) continue;
            const auto d=date(row.cells[*di]); const auto a=amount(row.cells[*ai]);
            if (!a) throw std::runtime_error("В календаре есть событие с неизвестной суммой");
            DividendEvent e; e.record_date=d;e.amount_per_share=*a;e.currency="RUB";e.source_url=url;
            if (pi && *pi<row.cells.size()) e.period=row.cells[*pi];
            if (bi && *bi<row.cells.size()) e.reported_last_buy=date(row.cells[*bi]);
            if (contains(row.markup,"dividend_canceled") || contains(row.markup,"dividend_cancelled")) e.status=DividendStatus::Cancelled;
            else if (d.empty() || contains(row.cells[*ai],"П") || contains(row.cells[*di],"П")) e.status=DividendStatus::Forecast;
            else if (contains(row.markup,"dividend_approved")) e.status=DividendStatus::Recommended;
            else e.status=history ? DividendStatus::Unknown : DividendStatus::Forecast;
            result.push_back(std::move(e));
        }
    }
    if (!recognized) throw std::runtime_error("Источник изменил формат календаря; сохранённые события оставлены");
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.record_date<b.record_date;});
    return result;
}

std::vector<DividendEvent> parse_lukoil_dividends(std::string_view html) {
    std::vector<DividendEvent> result; bool recognized=false;
    for(const auto& table:tables(html)) {
        bool valid=false;
        for(const auto& row:table) {
            if(row.header) {
                valid=row.cells.size()==5 && contains(row.cells[2],"Дивиденд на 1 акцию") && contains(row.cells[4],"имеющие право");
                recognized=recognized||valid;continue;
            }
            if(!valid || row.cells.size()!=5)continue;
            const auto d=date(row.cells[4]);const auto a=amount(row.cells[2]);
            if(d.empty()||!a)continue;
            DividendEvent e;e.record_date=d;e.amount_per_share=*a;e.currency="RUB";
            e.status=DividendStatus::Approved;e.period=row.cells[1];result.push_back(std::move(e));
        }
    }
    if(!recognized)throw std::runtime_error("Изменён формат официальной таблицы ЛУКОЙЛ");
    std::vector<DividendEvent> combined;
    for(const auto& e:result) {
        const auto it=std::find_if(combined.begin(),combined.end(),[&](const auto& a){return a.record_date==e.record_date && a.currency==e.currency;});
        if(it==combined.end())combined.push_back(e);
        else {it->amount_per_share+=e.amount_per_share;it->period+=" + "+e.period;}
    }
    return combined;
}

void reconcile_dividends(std::vector<DividendEvent>& events,const std::vector<DividendEvent>& official,
                         std::string_view url,std::string_view checked) {
    for(auto& e:events) {
        e.issuer_url=url;e.issuer_checked_at=checked;
        const auto it=std::find_if(official.begin(),official.end(),[&](const auto& o){return o.record_date==e.record_date && o.currency==e.currency;});
        if(it==official.end()) {e.verification=DividendVerification::NotChecked;continue;}
        double source_total=0.0;
        for(const auto& source:events)if(source.record_date==e.record_date && source.currency==e.currency && source.status!=DividendStatus::Cancelled)source_total+=source.amount_per_share;
        if(e.status!=DividendStatus::Cancelled && std::abs(it->amount_per_share-source_total)<0.000001) {
            e.verification=DividendVerification::Matched;e.status=DividendStatus::Approved;
        } else e.verification=DividendVerification::Conflict;
    }
    for(const auto& o:official) {
        if(std::any_of(events.begin(),events.end(),[&](const auto& e){return e.record_date==o.record_date && e.currency==o.currency;}))continue;
        auto e=o;e.source_url=url;e.issuer_url=url;e.issuer_checked_at=checked;e.verification=DividendVerification::Matched;
        events.push_back(std::move(e));
    }
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.record_date<b.record_date;});
}

void merge_dividends(std::vector<DividendEvent>& events,const std::vector<DividendEvent>& incoming) {
    for(const auto& e:incoming) {
        const auto it=std::find_if(events.begin(),events.end(),[&](const auto& a){return a.record_date==e.record_date && a.period==e.period && a.currency==e.currency;});
        if(it==events.end())events.push_back(e);else *it=e;
    }
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.record_date<b.record_date;});
}

std::vector<TradingDay> moex_calendar_2026() {
    // Official annual schedule: https://www.moex.com/n96571 and /n95564.
    // Reviewed 2026-10-15. This deliberately has no rules for another year.
    const std::vector<std::string> closed={"2026-01-01","2026-01-02","2026-01-07","2026-01-03","2026-01-04",
      "2026-01-10","2026-01-11","2026-02-14","2026-02-15","2026-03-07","2026-03-08","2026-03-21","2026-03-22",
      "2026-05-09","2026-05-10","2026-06-20","2026-06-21","2026-08-01","2026-08-02","2026-08-15","2026-08-16",
      "2026-09-12","2026-09-13","2026-10-24","2026-10-25","2026-12-05","2026-12-06","2026-12-31"};
    const std::vector<std::string> additional={"2026-02-23","2026-05-01","2026-06-12","2026-11-04"};
    std::vector<TradingDay> result;
    for(auto d=*day("2026-01-01");d<=*day("2026-12-31");d+=std::chrono::days{1}) {
        const auto key=iso(d);const auto weekday=std::chrono::weekday{d}.iso_encoding();
        TradingDay row;row.date=key;row.traded=std::find(closed.begin(),closed.end(),key)==closed.end();
        if(row.traded && (weekday>=6 || std::find(additional.begin(),additional.end(),key)!=additional.end()))row.session_date="pending";
        result.push_back(std::move(row));
    }
    for(std::size_t i=0;i<result.size();++i) {
        if(result[i].session_date!="pending")continue;
        const auto next=std::find_if(result.begin()+static_cast<std::ptrdiff_t>(i+1),result.end(),[](const auto& r){return r.traded && r.session_date.empty();});
        if(next!=result.end())result[i].session_date=next->date;else result[i].traded=false;
    }
    return result;
}

std::string dividend_status_text(DividendStatus s) {
    switch(s) {
    case DividendStatus::Forecast:return "Прогноз";
    case DividendStatus::Recommended:return "Рекомендовано СД";
    case DividendStatus::Approved:return "Утверждено";
    case DividendStatus::Cancelled:return "Отменено";
    default:return "История; утверждение не сверено";
    }
}

std::string serialize_dividend_snapshot(const DividendSnapshot& s) {
    std::ostringstream out;out<<"MOEXWatch-dividends-v1;"<<s.ticker<<';'<<s.checked_at<<'\n'<<std::setprecision(17);
    for(const auto& e:s.events)out<<e.record_date<<';'<<e.amount_per_share<<';'<<e.currency<<';'<<static_cast<int>(e.status)<<';'
      <<quoted(e.period)<<';'<<quoted(e.source_url)<<';'<<e.reported_last_buy<<';'<<e.last_buy_date<<';'<<quoted(e.last_buy_basis)<<';'
      <<quoted(e.issuer_url)<<';'<<e.issuer_checked_at<<';'<<static_cast<int>(e.verification)<<'\n';
    return out.str();
}
std::optional<DividendSnapshot> parse_dividend_snapshot(std::string_view csv,std::string_view ticker) {
    if(csv.size()>1024U*1024U)return {};
    const auto rows=parse_csv(csv);
    if(rows.empty()||rows[0].size()!=3||rows[0][0]!="MOEXWatch-dividends-v1"||rows[0][1]!=ticker||!day(rows[0][2].substr(0,10)))return {};
    DividendSnapshot s;s.ticker=ticker;s.checked_at=rows[0][2];
    for(std::size_t i=1;i<rows.size();++i) {
        const auto& r=rows[i];if(r.size()!=12)return {};
        const auto n=amount(r[1]);
        if((!r[0].empty()&&!day(r[0]))||!n||r[2]!="RUB"||r[3].size()!=1||r[3][0]<'0'||r[3][0]>'4'||r[11].size()!=1||r[11][0]<'0'||r[11][0]>'3')return {};
        if((!r[6].empty()&&!day(r[6]))||(!r[7].empty()&&!day(r[7])))return {};
        DividendEvent e;e.record_date=r[0];e.amount_per_share=*n;e.currency=r[2];e.status=static_cast<DividendStatus>(r[3][0]-'0');
        e.period=r[4];e.source_url=r[5];e.reported_last_buy=r[6];e.last_buy_date=r[7];e.last_buy_basis=r[8];e.issuer_url=r[9];e.issuer_checked_at=r[10];e.verification=static_cast<DividendVerification>(r[11][0]-'0');
        if(e.status==DividendStatus::Approved && e.verification!=DividendVerification::Matched)return {};
        s.events.push_back(std::move(e));
    }
    return s;
}
} // namespace moexwatch
