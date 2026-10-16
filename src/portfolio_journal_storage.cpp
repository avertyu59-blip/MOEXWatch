#include "storage.hpp"
#include "portfolio_identity.hpp"
#include <charconv>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace moexwatch::storage {
namespace {
std::optional<double> parse_finite_number(std::string_view text) {
    double value{};const auto [end,err]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(err!=std::errc{}||end!=text.data()+text.size()||!std::isfinite(value))return {};
    return value;
}
constexpr auto filename="portfolio_journal_v2.csv";
std::uint64_t integer(std::string_view text) {
    std::uint64_t value{};const auto [end,err]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(err!=std::errc{}||end!=text.data()+text.size())throw std::runtime_error("Некорректный порядок или счётчик журнала");
    return value;
}
std::string encode(const PortfolioJournal& journal) {
    if(!valid_portfolio_journal(journal))throw std::runtime_error("Несогласованные ID, операции или уровни журнала");
    auto watermark=journal.sequence_watermark;
    for(const auto& t:journal.transactions)watermark=std::max(watermark,t.sequence);
    std::ostringstream out;out<<std::setprecision(17)<<"MOEXWATCH_PORTFOLIO;2\nSEQUENCE;"<<watermark<<"\nTRANSACTIONS\ndate;ticker;side;quantity;price;commission;id;position_id;sequence\n";
    for(const auto& t:journal.transactions)out<<t.date<<';'<<t.ticker<<';'<<(t.side==PortfolioSide::Buy?"BUY":"SELL")<<';'<<t.quantity<<';'<<t.price<<';'<<t.commission<<';'<<t.id<<';'<<t.position_id<<';'<<t.sequence<<'\n';
    const auto levels=[&](std::string_view section,const auto& values){
        out<<section<<"\nticker;opened_on;stop_loss;take_profit;position_id\n";
        for(const auto& l:values){out<<l.ticker<<';'<<l.opened_on<<';';if(l.stop_loss)out<<*l.stop_loss;out<<';';if(l.take_profit)out<<*l.take_profit;out<<';'<<l.position_id<<'\n';}
    };
    levels("LEVELS",journal.levels);levels("UNRESOLVED_LEVELS",journal.unresolved_levels);
    out<<"END;"<<journal.transactions.size()<<';'<<journal.levels.size()<<';'<<journal.unresolved_levels.size()<<'\n';
    return out.str();
}
PortfolioJournal decode(std::string_view bytes) {
    const auto rows=parse_csv(bytes);std::size_t i=0;
    const auto require=[&](const std::vector<std::string>& expected){if(i>=rows.size()||rows[i++]!=expected)throw std::runtime_error("Неизвестный или неполный формат portfolio_journal_v2.csv");};
    require({"MOEXWATCH_PORTFOLIO","2"});
    if(i>=rows.size()||rows[i].size()!=2||rows[i][0]!="SEQUENCE")throw std::runtime_error("Нет счётчика порядка журнала");
    PortfolioJournal journal;journal.sequence_watermark=integer(rows[i++][1]);
    require({"TRANSACTIONS"});
    require({"date","ticker","side","quantity","price","commission","id","position_id","sequence"});
    for(;i<rows.size()&&rows[i]!=std::vector<std::string>{"LEVELS"};++i){
        const auto& r=rows[i];if(r.size()!=9)throw std::runtime_error("Неполная операция журнала v2");
        const auto q=parse_finite_number(r[3]),p=parse_finite_number(r[4]),fee=parse_finite_number(r[5]);
        if(!q||!p||!fee||(r[2]!="BUY"&&r[2]!="SELL"))throw std::runtime_error("Некорректная операция журнала v2");
        journal.transactions.push_back({r[0],r[1],r[2]=="BUY"?PortfolioSide::Buy:PortfolioSide::Sell,*q,*p,*fee,r[6],r[7],integer(r[8])});
    }
    const auto levels=[&](std::string_view section,std::string_view next,auto& values){
        require({std::string(section)});require({"ticker","opened_on","stop_loss","take_profit","position_id"});
        for(;i<rows.size()&&!(rows[i].size()>=1&&rows[i][0]==next);++i){
            const auto& r=rows[i];if(r.size()!=5)throw std::runtime_error("Неполная запись уровней v2");
            const auto stop=r[2].empty()?std::optional<double>{}:parse_finite_number(r[2]);
            const auto take=r[3].empty()?std::optional<double>{}:parse_finite_number(r[3]);
            if((!r[2].empty()&&!stop)||(!r[3].empty()&&!take))throw std::runtime_error("Некорректные уровни v2");
            values.push_back({r[0],r[1],stop,take,r[4]});
        }
    };
    levels("LEVELS","UNRESOLVED_LEVELS",journal.levels);levels("UNRESOLVED_LEVELS","END",journal.unresolved_levels);
    if(i+1!=rows.size()||rows[i].size()!=4||rows[i][0]!="END"||integer(rows[i][1])!=journal.transactions.size()||
        integer(rows[i][2])!=journal.levels.size()||integer(rows[i][3])!=journal.unresolved_levels.size()||!valid_portfolio_journal(journal))
        throw std::runtime_error("Повреждён журнал v2: проверьте счётчики, ID и связи; старые файлы не подставляются автоматически");
    for(const auto& t:journal.transactions)if(t.sequence>journal.sequence_watermark)throw std::runtime_error("Повреждён счётчик порядка");
    return journal;
}
}
void save_portfolio_journal(const std::filesystem::path& root,const PortfolioJournal& journal) {
    const auto bytes=encode(journal);
    if(bytes.size()>4U*1024U*1024U)throw std::runtime_error("Журнал превышает 4 МБ; запись отменена, исходный файл сохранён");
    atomic_write(root/filename,bytes);
}
PortfolioJournal load_portfolio_journal(const std::filesystem::path& root,bool allow_migration) {
    if(const auto bytes=read_text(root/filename))return decode(*bytes);
    const auto legacy_operations=read_text(root/"portfolio_transactions.csv");
    const auto legacy_levels=read_text(root/"position_levels.csv");
    if(!legacy_operations&&!legacy_levels)return {};
    if(!allow_migration)throw std::runtime_error("Миграция портфеля приостановлена до исправления ошибки пользовательских файлов");
    std::vector<PortfolioTransaction> operations;std::vector<PositionLevels> levels;
    load_portfolio(root,operations);load_position_levels(root,levels);
    auto journal=migrate_portfolio_identity(operations,levels);
    if(read_text(root/"portfolio_transactions.csv")!=legacy_operations||read_text(root/"position_levels.csv")!=legacy_levels)
        throw std::runtime_error("Старые файлы изменились во время миграции; повторите запуск");
    create_backup(root);
    if(read_text(root/filename)||read_text(root/"portfolio_transactions.csv")!=legacy_operations||read_text(root/"position_levels.csv")!=legacy_levels)
        throw std::runtime_error("Файлы изменились во время резервирования; миграция отменена");
    save_portfolio_journal(root,journal);return journal;
}
}
