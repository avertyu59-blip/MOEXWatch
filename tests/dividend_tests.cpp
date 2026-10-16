#include "market_core.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
using namespace moexwatch;
namespace {
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
std::string fixture(const char* file){std::ifstream f(std::string(FIXTURE_DIR)+"/"+file);if(!f)throw std::runtime_error("fixture missing");return {(std::istreambuf_iterator<char>(f)),{}};}
template<class F> bool throws(F&& f){try{f();return false;}catch(const std::runtime_error&){return true;}}
std::string row(std::string date,std::string amount,std::string cls=""){
 return "<tr class='"+cls+"'><td>GAZP</td><td>15.10.2026</td><td>"+date+"</td><td>2026 год</td><td>"+amount+"</td></tr>";
}
const std::string header="<tr><th>Тикер</th><th>дата T-1</th><th>дата отсечки</th><th>Период</th><th>дивиденд</th></tr>";
}
int main(){
 const auto gazp=parse_smartlab_dividends(fixture("smartlab_gazp.html"),"GAZP","https://smart-lab.ru/q/GAZP/dividend/");
 check(gazp.size()==3,"real GAZP history parsed");
 check(gazp[1].status==DividendStatus::Cancelled && std::abs(gazp[1].amount_per_share-52.53)<1e-8,"cancelled GAZP payment retained as cancelled");
 check(gazp[2].record_date=="2022-10-11" && gazp[2].reported_last_buy=="2022-10-07","T+2 source date retained, not calculated by subtracting a day");
 const auto sber=parse_smartlab_dividends(fixture("smartlab_sber.html"),"SBER","https://smart-lab.ru/");
 check(sber.size()==3 && sber.back().status==DividendStatus::Unknown,"history is not proof of shareholder approval");
 check(parse_smartlab_dividends(fixture("smartlab_sber.html"),"SBERP","https://smart-lab.ru/").empty(),"preferred and ordinary shares separated");
 auto calendar=parse_smartlab_dividends(fixture("smartlab_calendar.html"),"LKOH","https://smart-lab.ru/dividends/");
 check(calendar.size()==1 && calendar[0].status==DividendStatus::Recommended,"green calendar row is board recommendation, not approval");
 check(parse_smartlab_dividends("<table>"+header+"</table>","GAZP","u").empty(),"recognized empty table accepted");
 check(throws([]{parse_smartlab_dividends("<html>Access denied</html>","GAZP","u");}),"HTTP 200 access wall is not an empty calendar");
 check(throws([]{parse_smartlab_dividends("description\nSECID;GAZP\nboards\nTQBR", "GAZP","u");}),"original unsupported ISS response rejected");
 check(throws([]{parse_smartlab_dividends("<table>"+header+row("16.10.2026","nan")+"</table>","GAZP","u");}),"malformed amount rejected");
 auto forecast=parse_smartlab_dividends("<table>"+header+row("16.10.2026 П","12,5 П","dividend_approved")+"</table>","GAZP","u");
 check(forecast[0].status==DividendStatus::Forecast,"forecast marker overrides green class");
 auto undated=parse_smartlab_dividends("<table>"+header+row("n/a","12,5 П")+"</table>","GAZP","u");
 check(undated.size()==1 && undated[0].record_date.empty() && undated[0].status==DividendStatus::Forecast,"undated forecast preserved");
 const auto official=parse_lukoil_dividends(fixture("lukoil_official.html"));
 check(official.size()==2 && official[0].record_date=="2026-01-12" && official[0].amount_per_share==397,"official Russian date and per-share amount parsed");
 auto approved=official;for(auto&e:approved)e.status=DividendStatus::Recommended;
 reconcile_dividends(approved,official,"https://lukoil.ru/InvestorAndShareholderCenter/Securities/Dividends","2026-10-15 20:00");
 check(approved[0].status==DividendStatus::Approved && approved[0].verification==DividendVerification::Matched,"approval requires matching official date and amount");
 auto wrong=official;wrong[0].amount_per_share+=1;wrong[0].status=DividendStatus::Forecast;
 reconcile_dividends(wrong,official,"https://lukoil.ru/","2026-10-15");
 check(wrong[0].verification==DividendVerification::Conflict && wrong[0].status==DividendStatus::Forecast,"mismatch not silently overwritten or promoted");
 auto combined="<table><tr><th>Год</th><th>Период</th><th>Дивиденд на 1 акцию</th><th>Всего</th><th>имеющие право</th></tr>";
 const auto grouped=parse_lukoil_dividends(std::string(combined)+"<tr><td>2022</td><td>2021</td><td>256</td><td>100000000</td><td>21 декабря 2022 года</td></tr><tr><td>2022</td><td>9м2022</td><td>537</td><td>200000000</td><td>21 декабря 2022 года</td></tr></table>");
 check(grouped.size()==1 && grouped[0].amount_per_share==793,"same-day official payments aggregated without confusing total and per-share amount");
 auto snapshot=DividendSnapshot{"LKOH","2026-10-15 20:00",approved};
 snapshot.events[0].period="период; с \"кавычками\"";
 const auto csv=serialize_dividend_snapshot(snapshot);const auto restored=parse_dividend_snapshot(csv,"LKOH");
 check(restored && restored->events.size()==2 && restored->events[0].period==snapshot.events[0].period && restored->events[0].issuer_url==snapshot.events[0].issuer_url,"disk snapshot round trip preserves provenance and quotes");
 check(!parse_dividend_snapshot(csv,"GAZP"),"wrong ticker cache rejected");
 check(!parse_dividend_snapshot(csv+"broken;row\n","LKOH"),"truncated cache rejected as a whole");
 check(parse_dividend_snapshot(serialize_dividend_snapshot({"GAZP","2026-10-15",undated}),"GAZP").has_value(),"undated forecasts survive restart");
 const auto days=moex_calendar_2026();
 check(days.size()==365,"full annual calendar coverage");
 check(dividend_last_buy("2026-10-19",days)=="2026-10-16","Monday registry, Friday purchase; weekend sessions do not add settlement days");
 check(dividend_last_buy("2026-10-18",days)=="2026-10-15","Sunday registry settles by Friday, not Monday");
 check(dividend_last_buy("2026-11-05",days)=="2026-11-03","holiday additional session belongs to next business date");
 check(dividend_last_buy("2026-01-12",days)=="2026-01-09","January trading holidays use exchange calendar");
 check(!dividend_last_buy("2027-01-12",days),"unknown year not guessed");
 check(!dividend_last_buy("2026-02-30",days),"invalid calendar date rejected");
 auto missing=days;missing.erase(std::remove_if(missing.begin(),missing.end(),[](const auto&d){return d.date=="2026-10-18";}),missing.end());
 check(!dividend_last_buy("2026-10-19",missing),"gap before registry does not silently become a holiday");
 // T+2 transition is explicit for calendars supplied for historical years.
 std::vector<TradingDay> old{{"2023-07-25",true,""},{"2023-07-26",true,""},{"2023-07-27",true,""},{"2023-07-28",true,""}};
 check(dividend_last_buy("2023-07-28",old)=="2023-07-26","historical T+2 uses two settlement days");
 auto merge=calendar;merge_dividends(merge,calendar);check(merge.size()==calendar.size(),"duplicate calendar rows do not grow on refresh");
 std::cout<<"dividend_tests: all checks passed\n";
}
