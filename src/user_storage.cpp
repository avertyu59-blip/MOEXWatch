#include "storage.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
namespace moexwatch::storage {
namespace {
std::recursive_mutex user_mutex;
constexpr std::size_t limit=4U*1024U*1024U;
std::uint32_t checksum(std::string_view data){std::uint32_t crc=0xffffffffU;for(unsigned char c:data){crc^=c;for(int i=0;i<8;++i)crc=(crc>>1)^((crc&1U)?0xedb88320U:0U);}return ~crc;}
std::string envelope(std::string_view kind,std::string_view payload){
    std::ostringstream out;out<<"MOEXWATCH_"<<kind<<";1;"<<payload.size()<<';'<<std::hex<<std::setfill('0')<<std::setw(8)<<checksum(payload)<<'\n'<<payload;
    auto result=out.str();if(result.size()>limit)throw std::runtime_error("Пользовательский файл превышает 4 МБ");return result;
}
std::vector<std::vector<std::string>> strict_csv(std::string_view payload){
    enum class Cell {Start,Plain,Quoted,Closed};Cell state=Cell::Start;
    for(std::size_t i=0;i<payload.size();++i){
        const char c=payload[i];
        if(state==Cell::Quoted){if(c=='"')state=Cell::Closed;continue;}
        if(state==Cell::Closed&&c=='"'){state=Cell::Quoted;continue;}
        if(c==';'||c=='\n'||c=='\r'){state=Cell::Start;continue;}
        if(state==Cell::Closed||(c=='"'&&state!=Cell::Start))throw std::runtime_error("Повреждены кавычки CSV");
        state=c=='"'?Cell::Quoted:Cell::Plain;
    }
    if(state==Cell::Quoted)throw std::runtime_error("Незакрытые кавычки CSV");
    return parse_csv(payload);
}
struct Document {std::string payload;bool legacy{};};
Document decode(std::string_view bytes,std::string_view kind){
    if(!bytes.starts_with("MOEXWATCH_"))return {std::string(bytes),true};
    const auto newline=bytes.find('\n');if(newline==bytes.npos)throw std::runtime_error("Оборван версионный заголовок");
    const auto rows=parse_csv(bytes.substr(0,newline));
    if(rows.size()!=1||rows[0].size()!=4||rows[0][0]!=std::string("MOEXWATCH_")+std::string(kind)||rows[0][1]!="1")throw std::runtime_error("Неизвестная версия или тип пользовательского файла");
    std::size_t size{};std::uint32_t crc{};const auto& length=rows[0][2];const auto& sum=rows[0][3];
    const auto [end,err]=std::from_chars(length.data(),length.data()+length.size(),size);
    const auto [ce,ec]=std::from_chars(sum.data(),sum.data()+sum.size(),crc,16);
    const auto payload=bytes.substr(newline+1);
    if(err!=std::errc{}||end!=length.data()+length.size()||ec!=std::errc{}||ce!=sum.data()+sum.size()||sum.size()!=8||size!=payload.size()||crc!=checksum(payload))throw std::runtime_error("Нарушена целостность пользовательского файла");
    return {std::string(payload),false};
}
void write_document(const std::filesystem::path& path,std::string_view kind,std::string_view payload){
    // Do not overwrite a future or damaged versioned file even if called without a prior load.
    if(const auto existing=read_text(path))if(existing->starts_with("MOEXWATCH_"))(void)decode(*existing,kind);
    atomic_write(path,envelope(kind,payload));
}
void migrate(const std::filesystem::path& root,const std::filesystem::path& path,std::string_view kind,const std::string& original,std::string_view payload,bool allowed){
    if(!allowed)return; // Read-only recovery mode can inspect a valid legacy file.
    const auto snapshot=create_backup(root);
    if(read_text(snapshot/path.filename())!=std::optional<std::string>(original)||read_text(path)!=std::optional<std::string>(original))throw std::runtime_error("Файл изменился во время миграции");
    atomic_write(path,envelope(kind,payload));
}
std::optional<double> number(std::string value){
    std::replace(value.begin(),value.end(),',','.');double result{};
    const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size()||!std::isfinite(result))return {};
    return result;
}
std::string signal_payload(const SignalSettings& value){
    if(!valid_signal_settings(value))throw std::runtime_error("Некорректные настройки сигналов");
    std::ostringstream out;out<<std::setprecision(17)
        <<"rsi_oversold_enabled="<<value.rsi_oversold_enabled<<'\n'<<"rsi_oversold_below="<<value.rsi_oversold_below<<'\n'
        <<"rsi_overbought_enabled="<<value.rsi_overbought_enabled<<'\n'<<"rsi_overbought_above="<<value.rsi_overbought_above<<'\n'
        <<"above_sma200_enabled="<<value.above_sma200_enabled<<'\n'<<"near_target_enabled="<<value.near_target_enabled<<'\n'
        <<"near_target_percent="<<value.near_target_percent<<'\n'<<"require_all="<<value.require_all<<'\n'
        <<"notifications_enabled="<<value.notifications_enabled<<'\n'<<"auto_refresh_enabled="<<value.auto_refresh_enabled<<'\n'
        <<"auto_refresh_minutes="<<value.auto_refresh_minutes<<'\n';return out.str();
}
std::string trade_payload(const std::vector<Trade>& value){
    std::ostringstream out;out<<"ticker;quantity;buy_price;sell_price;commission;tax_rate_percent\n"<<std::setprecision(17);
    for(const auto& t:value){if(!valid_trade(t))throw std::runtime_error("Некорректная сделка");out<<t.ticker<<';'<<t.quantity<<';'<<t.buy_price<<';'<<t.sell_price<<';'<<t.commission<<';'<<t.tax_rate_percent<<'\n';}return out.str();
}
std::string watch_payload(const std::vector<Instrument>& value){
    if(value.size()>30)throw std::runtime_error("Не более 30 бумаг в наблюдении");
    std::set<std::string> ids;std::ostringstream out;out<<"# ticker;target\n"<<std::setprecision(17);
    for(const auto& item:value){if(!valid_ticker(item.ticker)||!ids.insert(item.ticker).second||(item.target&&(!std::isfinite(*item.target)||*item.target<=0)))throw std::runtime_error("Некорректный тикер, цель или дубликат");out<<item.ticker<<';';if(item.target)out<<*item.target;out<<'\n';}return out.str();
}
}
void save_signal_settings(const std::filesystem::path& root,const SignalSettings& value){std::lock_guard lock(user_mutex);write_document(root/"signals.ini","SIGNALS",signal_payload(value));}
void load_signal_settings(const std::filesystem::path& root,SignalSettings& value,bool allow_migration){
    std::lock_guard lock(user_mutex);const auto path=root/"signals.ini";const auto bytes=read_text(path);if(!bytes)return;
    const auto doc=decode(*bytes,"SIGNALS");std::istringstream in(doc.payload);std::string line;std::map<std::string,std::string> fields;
    while(std::getline(in,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty())continue;
        const auto sep=line.find('=');if(sep==line.npos||sep==0||line.find('=',sep+1)!=line.npos||!fields.emplace(line.substr(0,sep),line.substr(sep+1)).second)throw std::runtime_error("Повреждена строка или повтор настройки");
    }
    SignalSettings loaded;std::set<std::string> known;
    const auto boolean=[&](const char* key,bool& destination){known.insert(key);const auto it=fields.find(key);if(it==fields.end()){if(!doc.legacy)throw std::runtime_error("Отсутствует обязательная настройка");return;}if(it->second!="0"&&it->second!="1")throw std::runtime_error("Некорректный переключатель настройки");destination=it->second=="1";};
    const auto numeric=[&](const char* key,double& destination){known.insert(key);const auto it=fields.find(key);if(it==fields.end()){if(!doc.legacy)throw std::runtime_error("Отсутствует обязательная настройка");return;}const auto parsed=number(it->second);if(!parsed)throw std::runtime_error("Некорректное число настройки");destination=*parsed;};
    boolean("rsi_oversold_enabled",loaded.rsi_oversold_enabled);numeric("rsi_oversold_below",loaded.rsi_oversold_below);
    boolean("rsi_overbought_enabled",loaded.rsi_overbought_enabled);numeric("rsi_overbought_above",loaded.rsi_overbought_above);
    boolean("above_sma200_enabled",loaded.above_sma200_enabled);boolean("near_target_enabled",loaded.near_target_enabled);numeric("near_target_percent",loaded.near_target_percent);
    boolean("require_all",loaded.require_all);boolean("notifications_enabled",loaded.notifications_enabled);boolean("auto_refresh_enabled",loaded.auto_refresh_enabled);
    double interval=loaded.auto_refresh_minutes;numeric("auto_refresh_minutes",interval);if(interval!=15&&interval!=30&&interval!=60)throw std::runtime_error("Некорректный интервал обновления");loaded.auto_refresh_minutes=static_cast<int>(interval);
    if(fields.empty()||std::any_of(fields.begin(),fields.end(),[&](const auto& f){return !known.contains(f.first);})||!valid_signal_settings(loaded))throw std::runtime_error("Неизвестная или недопустимая настройка");
    if(doc.legacy)migrate(root,path,"SIGNALS",*bytes,signal_payload(loaded),allow_migration);
    value=loaded;
}
void save_trades(const std::filesystem::path& root,const std::vector<Trade>& value){std::lock_guard lock(user_mutex);write_document(root/"trades.csv","TRADES",trade_payload(value));}
void load_trades(const std::filesystem::path& root,std::vector<Trade>& value,bool allow_migration){
    std::lock_guard lock(user_mutex);const auto path=root/"trades.csv";const auto bytes=read_text(path);if(!bytes)return;
    const auto doc=decode(*bytes,"TRADES");
    const auto rows=strict_csv(doc.payload);
    if(rows.empty()||rows[0]!=std::vector<std::string>{"ticker","quantity","buy_price","sell_price","commission","tax_rate_percent"})throw std::runtime_error("Неизвестный формат сделок");
    std::vector<Trade> loaded;
    for(std::size_t i=1;i<rows.size();++i){const auto& row=rows[i];if(row.size()!=6)throw std::runtime_error("Неполная строка сделки");const auto q=number(row[1]),b=number(row[2]),s=number(row[3]),c=number(row[4]),t=number(row[5]);if(!q||!b||!s||!c||!t)throw std::runtime_error("Некорректное число сделки");Trade trade{row[0],*q,*b,*s,*c,*t};if(!valid_trade(trade))throw std::runtime_error("Недопустимая сделка");loaded.push_back(trade);}
    if(doc.legacy)migrate(root,path,"TRADES",*bytes,trade_payload(loaded),allow_migration);
    value=std::move(loaded);
}
void save_watchlist(const std::filesystem::path& root,const std::vector<Instrument>& value){std::lock_guard lock(user_mutex);write_document(root/"watchlist.csv","WATCHLIST",watch_payload(value));}
void load_watchlist(const std::filesystem::path& root,std::vector<Instrument>& value,bool allow_migration){
    std::lock_guard lock(user_mutex);const auto path=root/"watchlist.csv";const auto bytes=read_text(path);
    if(!bytes){if(value.empty())for(const char* ticker:{"GAZP","LKOH","SBER","ROSN","OZON","T","YDEX","NVTK","SMLT","VTBR"}){Instrument item;item.ticker=ticker;item.quote.ticker=ticker;value.push_back(item);}return;}
    const auto doc=decode(*bytes,"WATCHLIST");
    const auto rows=strict_csv(doc.payload);
    if(rows.empty()||rows[0]!=std::vector<std::string>{"# ticker","target"})throw std::runtime_error("Неизвестный формат списка наблюдения");
    if(rows.size()>31)throw std::runtime_error("Список наблюдения превышает 30 бумаг");
    std::vector<Instrument> loaded;std::set<std::string> ids;
    for(std::size_t i=1;i<rows.size();++i){const auto& row=rows[i];if(row.size()!=2||!valid_ticker(row[0])||!ids.insert(row[0]).second)throw std::runtime_error("Некорректный тикер или дубликат");Instrument item;item.ticker=row[0];item.quote.ticker=item.ticker;if(!row[1].empty()){item.target=number(row[1]);if(!item.target||*item.target<=0)throw std::runtime_error("Некорректная целевая цена");}loaded.push_back(item);}
    if(doc.legacy)migrate(root,path,"WATCHLIST",*bytes,watch_payload(loaded),allow_migration);
    value=std::move(loaded);
}
}
