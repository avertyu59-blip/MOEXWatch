#include "observation.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
namespace moexwatch {
namespace {
std::optional<std::int64_t> timestamp(std::string_view value){
 if(value.size()!=19||value[4]!='-'||value[7]!='-'||(value[10]!=' '&&value[10]!='T')||value[13]!=':'||value[16]!=':')return {};
 for(std::size_t i=0;i<19;++i)if(i!=4&&i!=7&&i!=10&&i!=13&&i!=16&&(value[i]<'0'||value[i]>'9'))return {};
 const auto part=[&](std::size_t p,std::size_t n){return std::stoi(std::string(value.substr(p,n)));};
 const auto y=part(0,4),m=part(5,2),d=part(8,2),h=part(11,2),minute=part(14,2),s=part(17,2);
 const std::chrono::year_month_day day{std::chrono::year{y},std::chrono::month{static_cast<unsigned>(m)},std::chrono::day{static_cast<unsigned>(d)}};
 if(y<1990||y>2100||!day.ok()||h>23||minute>59||s>59)return {};
 return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::sys_days{day}.time_since_epoch()).count()+h*3600+minute*60+s-3*3600;
}
std::optional<std::string> quote_timestamp(const Quote& q){
 const auto source=timestamp(q.source_timestamp);
 if(!source||q.updated_at.size()!=8)return {};
 std::string candidate=q.source_timestamp.substr(0,10)+" "+q.updated_at;
 const auto last=timestamp(candidate);if(!last)return {};
 if(*last>*source+90)candidate=observation_time(*last-24*3600);
 return candidate;
}
bool valid_rule(const PriceAlert&r){return r.id>0&&valid_ticker(r.ticker)&&std::isfinite(r.level)&&r.level>0;}
std::string real(double value){std::ostringstream out;out<<std::setprecision(17)<<value;return out.str();}
std::string csv_quote(std::string_view s){std::string result="\"";for(char c:s){if(c=='"')result+='"';result+=c;}return result+'"';}
std::vector<std::vector<std::string>> data(std::string_view csv,std::string_view version){
 if(csv.size()>4U*1024U*1024U)throw std::runtime_error("Файл наблюдений превышает 4 МБ");
 bool in_quote=false;for(std::size_t i=0;i<csv.size();++i)if(csv[i]=='"'){if(in_quote&&i+1<csv.size()&&csv[i+1]=='"')++i;else in_quote=!in_quote;}
 if(in_quote)throw std::runtime_error("Повреждены кавычки файла наблюдений");
 auto rows=parse_csv(csv);if(rows.empty()||rows[0].size()!=1||rows[0][0]!=version)throw std::runtime_error("Неизвестная версия файла наблюдений");return rows;
}
std::optional<double> finite_number(std::string_view s){double value{};const auto[p,e]=std::from_chars(s.data(),s.data()+s.size(),value);if(e!=std::errc{}||p!=s.data()+s.size()||!std::isfinite(value))return {};return value;}
}
std::string observation_time(std::int64_t utc){
 const std::chrono::sys_seconds moscow{std::chrono::seconds{utc+3*3600}};
 const auto days=std::chrono::floor<std::chrono::days>(moscow);const std::chrono::year_month_day d{days};const std::chrono::hh_mm_ss time{moscow-days};
 std::ostringstream out;out<<int(d.year())<<'-'<<std::setfill('0')<<std::setw(2)<<unsigned(d.month())<<'-'<<std::setw(2)<<unsigned(d.day())<<' '<<std::setw(2)<<time.hours().count()<<':'<<std::setw(2)<<time.minutes().count()<<':'<<std::setw(2)<<time.seconds().count();return out.str();
}
bool same_technical_rules(const SignalSettings&a,const SignalSettings&b){return std::tie(a.rsi_oversold_enabled,a.rsi_oversold_below,a.rsi_overbought_enabled,a.rsi_overbought_above,a.above_sma200_enabled,a.near_target_enabled,a.near_target_percent,a.require_all)==std::tie(b.rsi_oversold_enabled,b.rsi_oversold_below,b.rsi_overbought_enabled,b.rsi_overbought_above,b.above_sma200_enabled,b.near_target_enabled,b.near_target_percent,b.require_all);}
bool fresh_moex_quote(const Quote&q,std::int64_t now){
 if(!q.last||!std::isfinite(*q.last)||*q.last<=0)return false;
 const auto source=timestamp(q.source_timestamp);const auto time=quote_timestamp(q);
 if(!source||!time)return false;
 const auto last=timestamp(*time);
 if(!last)return false;
 return now-*source>=-90&&now-*source<=20*60&&now-*last>=-90&&now-*last<=20*60;
}
void ObservationEngine::configure(std::vector<PriceAlert> rules){
 if(rules.size()>100)throw std::runtime_error("Не более 100 ценовых условий");
 std::set<std::uint64_t> ids;
 for(const auto&r:rules){if(!valid_rule(r)||!ids.insert(r.id).second)throw std::runtime_error("Некорректное или повторное ценовое условие");}
 std::map<std::uint64_t,bool> states;
 for(const auto&r:rules){const auto previous=std::find_if(rules_.begin(),rules_.end(),[&](const auto&p){return p.id==r.id&&p.ticker==r.ticker&&p.at_or_below==r.at_or_below&&p.level==r.level&&p.enabled==r.enabled;});if(previous!=rules_.end()&&price_state_.contains(r.id))states[r.id]=price_state_.at(r.id);}
 rules_=std::move(rules);price_state_=std::move(states);
}
void ObservationEngine::forget_ticker(std::string_view ticker){technical_state_.erase(std::string(ticker));for(const auto&r:rules_)if(r.ticker==ticker)price_state_.erase(r.id);}
std::vector<SignalEvent> ObservationEngine::observe(const Quote&q,bool fresh,bool available,const SignalEvaluation& technical,std::string_view mode,std::int64_t now){
 std::vector<SignalEvent> events;
 if(!fresh||!fresh_moex_quote(q,now))return events;
 const auto event=[&](std::string kind,std::string condition){events.push_back({observation_time(now),q.ticker,std::move(kind),std::move(condition),*q.last,*quote_timestamp(q),q.source_timestamp});};
 for(const auto&r:rules_){if(!r.enabled||r.ticker!=q.ticker)continue;const bool matched=r.at_or_below?*q.last<=r.level:*q.last>=r.level;const auto it=price_state_.find(r.id);if(it!=price_state_.end()&&!it->second&&matched)event("Цена","Условие #"+std::to_string(r.id)+": цена "+(r.at_or_below?"<= ":">= ")+real(r.level));price_state_[r.id]=matched;}
 if(available){const auto it=technical_state_.find(q.ticker);if(it!=technical_state_.end()&&!it->second&&technical.matched){std::string reason(mode);for(const auto&r:technical.reasons){if(!reason.empty())reason+="; ";reason+=r;}event("Технический",std::move(reason));}technical_state_[q.ticker]=technical.matched;}
 return events;
}
std::string serialize_price_alerts(const std::vector<PriceAlert>& rules){ObservationEngine validator;validator.configure(rules);std::ostringstream out;out<<"MOEXWatch-price-alerts-v1\nid;ticker;side;level;enabled\n";for(const auto&r:rules)out<<r.id<<';'<<r.ticker<<';'<<(r.at_or_below?"LE":"GE")<<';'<<real(r.level)<<';'<<(r.enabled?1:0)<<'\n';return out.str();}
std::vector<PriceAlert> parse_price_alerts(std::string_view csv){const auto rows=data(csv,"MOEXWatch-price-alerts-v1");if(rows.size()<2||rows[1]!=std::vector<std::string>{"id","ticker","side","level","enabled"})throw std::runtime_error("Неверный заголовок ценовых условий");std::vector<PriceAlert> result;
 for(std::size_t i=2;i<rows.size();++i){const auto&r=rows[i];if(r.size()!=5)throw std::runtime_error("Неполное ценовое условие");std::uint64_t id{};const auto[p,e]=std::from_chars(r[0].data(),r[0].data()+r[0].size(),id);const auto level=finite_number(r[3]);if(e!=std::errc{}||p!=r[0].data()+r[0].size()||!level||(r[2]!="LE"&&r[2]!="GE")||(r[4]!="0"&&r[4]!="1"))throw std::runtime_error("Неверные поля ценового условия");result.push_back({id,r[1],r[2]=="LE",*level,r[4]=="1"});}
 ObservationEngine validator;validator.configure(result);return result;
}
std::string serialize_signal_history(const std::vector<SignalEvent>& events){if(events.size()>2000)throw std::runtime_error("Не более 2000 событий истории");std::ostringstream out;out<<"MOEXWatch-signal-history-v1\ndetected_at;ticker;kind;condition;price;quote_time;source_time\n";for(const auto&e:events){if(!timestamp(e.detected_at)||!timestamp(e.quote_time)||!timestamp(e.source_time)||!valid_ticker(e.ticker)||!std::isfinite(e.price)||e.price<=0||e.condition.size()>4096||(e.kind!="Цена"&&e.kind!="Технический"))throw std::runtime_error("Некорректное событие истории");out<<e.detected_at<<';'<<e.ticker<<';'<<e.kind<<';'<<csv_quote(e.condition)<<';'<<real(e.price)<<';'<<e.quote_time<<';'<<e.source_time<<'\n';}return out.str();}
std::vector<SignalEvent> parse_signal_history(std::string_view csv){const auto rows=data(csv,"MOEXWatch-signal-history-v1");if(rows.size()<2||rows[1]!=std::vector<std::string>{"detected_at","ticker","kind","condition","price","quote_time","source_time"})throw std::runtime_error("Неверный заголовок истории сигналов");std::vector<SignalEvent> result;for(std::size_t i=2;i<rows.size();++i){const auto&r=rows[i];if(r.size()!=7)throw std::runtime_error("Неполное событие истории");const auto price=finite_number(r[4]);if(!price)throw std::runtime_error("Неверная цена истории");result.push_back({r[0],r[1],r[2],r[3],*price,r[5],r[6]});}serialize_signal_history(result);return result;}
bool optional_number_less(std::optional<double>a,std::optional<double>b,bool ascending){if(a&&!std::isfinite(*a))a.reset();if(b&&!std::isfinite(*b))b.reset();if(!a)return false;if(!b)return true;return ascending?*a<*b:*a>*b;}
std::optional<double> table_number(std::string_view text){std::string s(text);const auto begin=s.find_first_not_of(" \t");if(begin==std::string::npos)return {};s.erase(0,begin);std::replace(s.begin(),s.end(),',','.');double value{};const auto[p,e]=std::from_chars(s.data(),s.data()+s.size(),value);if(e!=std::errc{}||p==s.data()||!std::isfinite(value))return {};return value;}
}
