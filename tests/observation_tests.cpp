#include "observation.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace moexwatch;
namespace {void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}template<class F>bool fails(F&&f){try{f();return false;}catch(const std::exception&){return true;}}}
int main(){try{
 using namespace std::chrono;
 const auto now=duration_cast<seconds>(sys_days{year{2026}/10/15}.time_since_epoch()).count()+19*3600;
 Quote q{"SBER",200,{},"21:45:00","2026-10-15 22:00:00"};
 check(observation_time(now)=="2026-10-15 22:00:00","Moscow timestamp independent of OS timezone");
 check(fresh_moex_quote(q,now),"normal 15 minute delayed quote accepted");
 auto old=q;old.updated_at="20:00:00";check(!fresh_moex_quote(old,now),"fresh HTTP snapshot cannot conceal an old quote");
 old=q;old.source_timestamp="2026-10-14 22:00:00";check(!fresh_moex_quote(old,now),"previous day snapshot rejected");
 old=q;old.source_timestamp="";check(!fresh_moex_quote(old,now),"missing source date never counted fresh");
 old=q;old.source_timestamp="2026-02-30 22:00:00";check(!fresh_moex_quote(old,now),"invalid source date rejected");
 const auto midnight=now+2*3600+5*60;old=q;old.source_timestamp="2026-10-16 00:05:00";old.updated_at="23:50:00";check(fresh_moex_quote(old,midnight),"midnight quote uses previous trading calendar date");
 ObservationEngine engine;engine.configure({{1,"SBER",true,190,true},{2,"SBER",false,220,true}});
 SignalEvaluation matched;matched.matched=true;matched.reasons={"RSI14 28 < 30"};SignalEvaluation clear;
 check(engine.observe(q,false,false,clear,"Любое",now).empty(),"startup failure establishes no baseline");
 q.last=180;check(engine.observe(q,true,true,matched,"Любое",now).empty(),"first fresh match after startup establishes baseline only");
 q.last=170;check(engine.observe(q,true,true,matched,"Любое",now).empty(),"matching repeats do not notify");
 q.last=230;check(engine.observe(q,false,false,clear,"Любое",now).empty(),"stale opposite observation ignored");
 q.last=180;check(engine.observe(q,true,true,matched,"Любое",now).empty(),"network outage does not rearm matching signal");
 q.last=200;engine.observe(q,true,true,clear,"Любое",now);
 q.last=190;auto events=engine.observe(q,true,true,matched,"Любое",now);
 check(events.size()==2&&events[0].kind=="Цена"&&events[1].kind=="Технический","inclusive price crossing and technical transition recorded independently");
 check(events[0].price==190&&events[0].quote_time=="2026-10-15 21:45:00","history stores actual observed price and source quote time");
 const auto bytes=serialize_signal_history(events);auto restored=parse_signal_history(bytes);check(restored.size()==2&&restored[1].condition==events[1].condition,"history round trip preserves evidence");
 restored[0].condition="Уровень; \"пример\"\nстрока";check(parse_signal_history(serialize_signal_history(restored))[0].condition==restored[0].condition,"quoted history text round trip");
 SignalSettings a,b;b.notifications_enabled=!a.notifications_enabled;b.auto_refresh_minutes=60;
 check(same_technical_rules(a,b),"turning popups off or changing polling preserves technical baseline");
 b.rsi_oversold_below=20;check(!same_technical_rules(a,b),"threshold changes reset technical baseline");
 const auto rules=engine.rules();check(parse_price_alerts(serialize_price_alerts(rules)).size()==2,"rule settings survive restart");
 ObservationEngine restart;restart.configure(parse_price_alerts(serialize_price_alerts(rules)));check(restart.observe(q,true,true,matched,"Любое",now).empty(),"restart does not replay old history or current matches");
 engine.configure({{1,"SBER",true,195,true},{2,"SBER",false,220,true}});q.last=194;check(engine.observe(q,true,false,clear,"",now).empty(),"edited threshold gets a new baseline");
 q.last=225;auto upper=engine.observe(q,true,false,clear,"",now);check(upper.size()==1&&upper[0].condition.find(">= 220")!=std::string::npos,"independent upper crossing works");
 engine.configure({{1,"SBER",true,195,false}});q.last=180;check(engine.observe(q,true,false,clear,"",now).empty(),"disabled rule produces nothing");
 engine.configure({{1,"SBER",true,195,true}});check(engine.observe(q,true,false,clear,"",now).empty(),"reenabling a rule baselines instead of firing");
 engine.forget_ticker("SBER");check(engine.observe(q,true,true,matched,"",now).empty(),"readding removed ticker starts fresh");
 check(fails([&]{engine.configure({{1,"SBER",true,1,true},{1,"SBER",false,2,true}});}),"duplicate rule ids rejected");
 check(fails([]{parse_price_alerts("<html>error</html>");}),"invalid persisted settings not treated as empty");
 check(fails([&]{parse_signal_history(bytes+"broken;row\n");}),"incomplete history rejected");
 check(optional_number_less(2,10,true)&&optional_number_less(10,2,false),"numbers sort numerically in both directions");
 check(optional_number_less(1,{},true)&&optional_number_less(1,{},false)&&!optional_number_less({},1,true)&&!optional_number_less({},1,false),"missing values always last");
 check(!optional_number_less(std::numeric_limits<double>::quiet_NaN(),1,true),"nonfinite sort keys count as missing");
 check(table_number(" 12,5% *")==12.5&&!table_number("—")&&!table_number("н/д"),"formatted numeric values parsed without fake zeros");
 std::cout<<"observation_tests: all checks passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
