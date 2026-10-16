#include "storage.hpp"
#include "io_runtime.hpp"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <map>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace moexwatch::storage {
namespace {
std::recursive_mutex io_mutex;
std::optional<double> parse_finite_number(std::string value) {
    std::replace(value.begin(), value.end(), ',', '.');
    if (value.empty()) return std::nullopt;
    double parsed{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() || !std::isfinite(parsed)) return std::nullopt;
    return parsed;
}

}
void save_portfolio(const std::filesystem::path& root, const std::vector<PortfolioTransaction>& value) {
    if(std::any_of(value.begin(),value.end(),[](const auto& t){return !t.id.empty()||!t.position_id.empty()||t.sequence;}))throw std::runtime_error("Журнал с ID сохраняется только через save_portfolio_journal");
    std::ostringstream out;
    out << "date;ticker;side;quantity;price;commission\n" << std::setprecision(17);
    for (const auto& transaction : value)
        out << transaction.date << ';' << transaction.ticker << ';'
            << (transaction.side == PortfolioSide::Buy ? "BUY" : "SELL") << ';'
            << transaction.quantity << ';' << transaction.price << ';' << transaction.commission << '\n';
    atomic_write(root / "portfolio_transactions.csv", out.str());
}

void load_portfolio(const std::filesystem::path& root, std::vector<PortfolioTransaction>& value) {
    const auto saved = read_text(root / "portfolio_transactions.csv");
    if (!saved) return;
    std::istringstream in(*saved);
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto rows = parse_csv(content);
    if (rows.empty() || rows.front() != std::vector<std::string>{"date","ticker","side","quantity","price","commission"}) throw std::runtime_error("Неизвестный формат portfolio_transactions.csv");
    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].size() != 6) throw std::runtime_error("Неожиданное число полей старого журнала операций");
        const auto quantity = parse_finite_number(rows[i][3]);
        const auto price = parse_finite_number(rows[i][4]);
        const auto commission = parse_finite_number(rows[i][5]);
        if (!quantity || !price || !commission) throw std::runtime_error("Некорректное число в портфеле");
        const auto side = rows[i][2] == "BUY" ? PortfolioSide::Buy :
            (rows[i][2] == "SELL" ? PortfolioSide::Sell : PortfolioSide::Buy);
        if (rows[i][2] != "BUY" && rows[i][2] != "SELL") throw std::runtime_error("Неизвестный тип операции");
        PortfolioTransaction transaction{rows[i][0], rows[i][1], side, *quantity, *price, *commission};
        if (!valid_portfolio_transaction(transaction)) throw std::runtime_error("Некорректная операция портфеля");
        value.push_back(std::move(transaction));
    }
    std::stable_sort(value.begin(), value.end(),
        [](const auto& a, const auto& b) { return a.date < b.date; });
}

void save_position_levels(const std::filesystem::path& root, const std::vector<PositionLevels>& value) {
    if(std::any_of(value.begin(),value.end(),[](const auto& l){return !l.position_id.empty();}))throw std::runtime_error("Уровни с ID сохраняются только в едином журнале");
    std::ostringstream out;
    out << "ticker;opened_on;stop_loss;take_profit\n" << std::setprecision(15);
    for (const auto& levels : value) {
        if (!valid_position_levels(levels) || (!levels.stop_loss && !levels.take_profit)) continue;
        out << levels.ticker << ';' << levels.opened_on << ';';
        if (levels.stop_loss) out << *levels.stop_loss;
        out << ';';
        if (levels.take_profit) out << *levels.take_profit;
        out << '\n';
    }
    atomic_write(root / "position_levels.csv", out.str());
}

void load_position_levels(const std::filesystem::path& root, std::vector<PositionLevels>& value) {
    const auto saved = read_text(root / "position_levels.csv");
    if (!saved) return;
    std::istringstream in(*saved);
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto rows = parse_csv(content);
    if (rows.empty() || rows.front() != std::vector<std::string>{"ticker","opened_on","stop_loss","take_profit"}) throw std::runtime_error("Неизвестный формат position_levels.csv");
    for (std::size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].size() != 4) throw std::runtime_error("Неожиданное число полей старых уровней");
        const auto stop = rows[i][2].empty() ? std::optional<double>{} : parse_finite_number(rows[i][2]);
        const auto take = rows[i][3].empty() ? std::optional<double>{} : parse_finite_number(rows[i][3]);
        if ((!rows[i][2].empty() && !stop) || (!rows[i][3].empty() && !take)) throw std::runtime_error("Некорректное число в уровнях позиции");
        PositionLevels levels{rows[i][0], rows[i][1], stop, take};
        if (!valid_position_levels(levels)) throw std::runtime_error("Некорректные уровни позиции");
        value.push_back(std::move(levels));
    }
}

std::filesystem::path data_directory() {
#ifdef _WIN32
    const DWORD length=GetEnvironmentVariableW(L"APPDATA",nullptr,0);
    if(length>1) {
        std::wstring path(length,L'\0');
        const auto written=GetEnvironmentVariableW(L"APPDATA",path.data(),length);
        if(written>0 && written<length){path.resize(written);return std::filesystem::path(path)/L"MOEXWatch";}
    }
#else
    if(const auto path=std::getenv("XDG_DATA_HOME"))return std::filesystem::path(path)/"MOEXWatch";
#endif
    // Do not silently move personal data into a random working directory.
    throw std::runtime_error("Не найдена папка пользовательских данных");
}

std::optional<std::string> read_text(const std::filesystem::path& path,std::size_t limit) {
    std::lock_guard lock(io_mutex);
    std::error_code ec;
    if(!std::filesystem::exists(path,ec)) {if(ec)throw std::runtime_error("Не удалось проверить файл данных");return {};}
    const auto size=std::filesystem::file_size(path,ec);
    if(ec || size>limit)throw std::runtime_error("Файл данных повреждён или превышает допустимый размер");
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("Не удалось открыть файл данных");
    std::string result(static_cast<std::size_t>(size),'\0');
    if(size && !in.read(result.data(),static_cast<std::streamsize>(size)))throw std::runtime_error("Не удалось прочитать файл данных целиком");
    return result;
}

void atomic_write(const std::filesystem::path& path,std::string_view content) {
    std::lock_guard lock(io_mutex);
    std::filesystem::create_directories(path.parent_path());
    static std::atomic<unsigned> temporary_sequence{};
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    auto temporary=path;temporary += ".tmp-"+std::to_string(stamp)+"-"+std::to_string(temporary_sequence++);
    try {
        {
            std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
            if(!out || !out.write(content.data(),static_cast<std::streamsize>(content.size())) || !out.flush())
                throw std::runtime_error("Не удалось записать временный файл данных");
            out.close();
            if(!out)throw std::runtime_error("Не удалось закрыть файл данных");
        }
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Не удалось заменить файл данных (Windows "+std::to_string(GetLastError())+")");
#else
        std::filesystem::rename(temporary,path);
#endif
    } catch(...) {std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}

std::filesystem::path create_backup(const std::filesystem::path& root,std::stop_token stop) {
    check_cancelled(stop);
    std::lock_guard lock(io_mutex);
    const auto backup_root=root/"backups";
    std::filesystem::create_directories(backup_root);
    const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    static std::atomic<unsigned> sequence{};
    const std::string name="MOEXWatch-"+std::to_string(stamp)+"-"+std::to_string(sequence++);
    const auto partial=backup_root/(name+".partial"), complete=backup_root/name;
    std::filesystem::create_directory(partial);
    try {
        std::vector<std::filesystem::path> files;
        for(const auto* name_part:{"portfolio_journal_v2.csv","watchlist.csv","trades.csv","portfolio_transactions.csv","position_levels.csv","signals.ini","price_alerts.csv","signal_history.csv"})
            if(std::filesystem::exists(root/name_part))files.push_back(root/name_part);
        if(std::filesystem::exists(root))for(const auto& entry:std::filesystem::directory_iterator(root)) {
            if(!entry.is_regular_file())continue;
            const auto utf8=entry.path().filename().u8string();
            const std::string filename(utf8.begin(),utf8.end());
            constexpr std::string_view prefix="dividends_";
            if(filename.starts_with(prefix) && filename.ends_with(".csv") &&
                valid_ticker(std::string_view(filename).substr(prefix.size(),filename.size()-prefix.size()-4)))files.push_back(entry.path());
        }
        if(files.empty())throw std::runtime_error("Нет сохранённых пользовательских файлов для копирования");
        std::ostringstream manifest;
        manifest<<"MOEXWatch backup 0.6.8\nЗакройте приложение перед восстановлением. Скопируйте файлы из этой папки в APPDATA/MOEXWatch.\nДля полной копии заменяйте набор файлов целиком. Если восстанавливаете старую копию без portfolio_journal_v2.csv, сначала перенесите текущий portfolio_journal_v2.csv в отдельную безопасную папку: иначе приложение продолжит читать его вместо старых файлов.\nФайлы watchlist.csv, trades.csv и signals.ini начиная с 0.6.8 содержат версию и CRC32. Для отката используйте полный снимок до миграции.\nКэш котировок не требуется для восстановления.\n\nfile;bytes\n";
        for(const auto& file:files) {
            check_cancelled(stop);
            if(std::filesystem::is_symlink(file))throw std::runtime_error("Символические ссылки в резервную копию не включаются");
            const auto bytes=read_text(file);
            if(!bytes)throw std::runtime_error("Файл исчез во время резервного копирования");
            atomic_write(partial/file.filename(),*bytes);
            if(read_text(partial/file.filename())!=bytes)throw std::runtime_error("Проверка резервной копии не пройдена");
            manifest<<file.filename().string()<<';'<<bytes->size()<<'\n';
        }
        check_cancelled(stop);
        atomic_write(partial/"RESTORE.txt",manifest.str());
        std::filesystem::rename(partial,complete);
        return complete;
    } catch(...) {std::error_code ignored;std::filesystem::remove_all(partial,ignored);throw;}
}

namespace {
std::filesystem::path candle_path(const std::filesystem::path& root,std::string_view ticker) {
    if(!valid_ticker(ticker))throw std::runtime_error("Некорректный тикер кэша свечей");
    return root/"cache"/(std::string(ticker)+"-"+(ticker=="IMOEX"?"SNDX":"TQBR")+"-D1.csv");
}
bool valid_candle(const Candle& c) {
    // The transaction date validator already checks leap years and calendar validity.
    if(c.date.size()<10 || !valid_portfolio_transaction({c.date.substr(0,10),"SBER",PortfolioSide::Buy,1,1,0}))return false;
    return std::isfinite(c.open)&&std::isfinite(c.high)&&std::isfinite(c.low)&&std::isfinite(c.close)&&std::isfinite(c.volume)
        && c.open>0 && c.close>0 && c.low>0 && c.high>=c.low && c.open>=c.low && c.open<=c.high
        && c.close>=c.low && c.close<=c.high && c.volume>=0;
}
void validate_candles(const std::vector<Candle>& candles) {
    if(candles.empty()||candles.size()>10000)throw std::runtime_error("Недопустимый размер истории свечей");
    std::string previous;
    for(const auto& c:candles) {
        const auto date=c.date.substr(0,10);
        if(!valid_candle(c)||(!previous.empty()&&date<=previous))throw std::runtime_error("Некорректные либо повторные дневные свечи");
        previous=date;
    }
}
}

std::optional<CandleCache> load_candles(const std::filesystem::path& root,std::string_view ticker) {
    const auto saved=read_text(candle_path(root,ticker));
    if(!saved)return {};
    const auto rows=parse_csv(*saved);
    const auto board=ticker=="IMOEX"?"SNDX":"TQBR";
    if(rows.size()<3 || rows[0].size()!=8 || rows[0][0]!="MOEXWatch-candles-v1" || rows[0][1]!=ticker || rows[0][2]!=board
       || rows[0][3]!="24" || rows[0][4]!="iss.moex.com" || rows[0][5].empty())throw std::runtime_error("Неверные метаданные кэша свечей");
    auto candles=parse_candles_csv(*saved);
    if(candles.size()!=rows.size()-2)throw std::runtime_error("Кэш свечей прочитан не полностью");
    validate_candles(candles);
    if(candles.back().date.substr(0,10)!=rows[0][6] || std::to_string(candles.size())!=rows[0][7])throw std::runtime_error("Кэш свечей обрезан");
    return CandleCache{std::string(ticker),rows[0][5],std::move(candles)};
}

void save_candles(const std::filesystem::path& root,const CandleCache& cache) {
    validate_candles(cache.candles);
    if(cache.checked_at.empty() || cache.checked_at.find_first_of(";\r\n")!=std::string::npos)throw std::runtime_error("Не указано время обновления свечей");
    std::ostringstream out;out<<std::setprecision(17);
    out<<"MOEXWatch-candles-v1;"<<cache.ticker<<';'<<(cache.ticker=="IMOEX"?"SNDX":"TQBR")<<";24;iss.moex.com;"<<cache.checked_at<<';'<<cache.candles.back().date.substr(0,10)<<';'<<cache.candles.size()<<'\n';
    out<<"begin;open;high;low;close;volume\n";
    for(const auto& c:cache.candles)out<<c.date.substr(0,10)<<';'<<c.open<<';'<<c.high<<';'<<c.low<<';'<<c.close<<';'<<c.volume<<'\n';
    atomic_write(candle_path(root,cache.ticker),out.str());
}

std::vector<Candle> merge_candles(const std::vector<Candle>& saved,const std::vector<Candle>& incoming,std::string_view from) {
    if(!saved.empty())validate_candles(saved);
    if(!incoming.empty())validate_candles(incoming);
    std::map<std::string,Candle> by_date;
    for(const auto& c:saved)if(c.date.substr(0,10)>=from)by_date[c.date.substr(0,10)]=c;
    for(const auto& c:incoming)if(c.date.substr(0,10)>=from)by_date[c.date.substr(0,10)]=c;
    std::vector<Candle> result;
    for(auto& [date,c]:by_date){c.date=date;result.push_back(std::move(c));}
    if(!result.empty())validate_candles(result);
    return result;
}

} // namespace moexwatch::storage
