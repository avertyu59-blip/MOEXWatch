#include "portfolio_csv.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
namespace moexwatch {
namespace {
std::string trim(std::string s){const auto a=s.find_first_not_of(" \r\n\t");return a==std::string::npos?"":s.substr(a,s.find_last_not_of(" \r\n\t")-a+1);}
std::string upper(std::string s){for(char& c:s)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));return s;}
using Key=std::tuple<std::string,std::string,PortfolioSide,double,double,double>;
Key key(const PortfolioTransaction& t){return {t.date,t.ticker,t.side,t.quantity,t.price,t.commission};}
std::optional<double> number(std::string s){s=trim(std::move(s));std::replace(s.begin(),s.end(),',','.');double n{};const auto[p,e]=std::from_chars(s.data(),s.data()+s.size(),n);if(e!=std::errc{}||p!=s.data()+s.size()||!std::isfinite(n))return {};return n;}
struct CsvRow{std::size_t line{};std::vector<std::string> cells;};
std::vector<CsvRow> strict_csv(std::string_view csv,char separator){
 std::vector<CsvRow> rows;CsvRow row;row.line=1;std::string cell;bool quoted=false,closed=false;std::size_t line=1;
 const auto finish_cell=[&]{row.cells.push_back(cell);cell.clear();closed=false;};
 for(std::size_t i=0;i<csv.size();++i){const char c=csv[i];
  if(c=='\0')throw std::runtime_error("CSV содержит нулевые байты. Сохраните файл в UTF-8, а не UTF-16.");
  if(quoted){if(c=='"'){if(i+1<csv.size()&&csv[i+1]=='"'){cell+='"';++i;}else{quoted=false;closed=true;}}else{cell+=c;if(c=='\n')++line;}continue;}
  if(c=='"'){if(!cell.empty()||closed)throw std::runtime_error("Некорректные кавычки в CSV");quoted=true;continue;}
  if(c==separator){finish_cell();continue;}
  if(c=='\n'||c=='\r'){
   if(c=='\r'&&i+1<csv.size()&&csv[i+1]=='\n')++i;
   finish_cell();if(row.cells.size()!=1||!trim(row.cells[0]).empty())rows.push_back(row);
   if(rows.size()>10001)throw std::runtime_error("В одном файле допустимо не более 10 000 операций");
   ++line;row=CsvRow{};row.line=line;continue;
  }
  if(closed){if(c==' '||c=='\t')continue;throw std::runtime_error("Лишние символы после закрывающей кавычки");}
  cell+=c;
 }
 if(quoted)throw std::runtime_error("Не закрыта кавычка в CSV");
 if(!cell.empty()||closed||!row.cells.empty()){finish_cell();rows.push_back(row);}
 if(rows.size()>10001)throw std::runtime_error("В одном файле допустимо не более 10 000 операций");
 return rows;
}
}
ImportPreview preview_portfolio_csv(std::string_view csv,const std::vector<PortfolioTransaction>& existing){
 ImportPreview result;
 try{
  if(csv.size()>4U*1024U*1024U)throw std::runtime_error("CSV превышает 4 МБ");
  if(csv.starts_with("\xEF\xBB\xBF"))csv.remove_prefix(3);
  const auto end=csv.find_first_of("\r\n");const auto header_line=csv.substr(0,end);
  const char separator=header_line.find(';')!=std::string_view::npos?';':',';
  const auto rows=strict_csv(csv,separator);
  if(rows.empty())throw std::runtime_error("Файл пуст. Нужна строка заголовков date;ticker;side;quantity;price;commission");
  const std::array<std::string,6> names{"DATE","TICKER","SIDE","QUANTITY","PRICE","COMMISSION"};
  std::array<std::size_t,6> columns{};std::set<std::string> headers;
  if(rows[0].cells.size()!=6)throw std::runtime_error("Ожидаются ровно 6 столбцов: date;ticker;side;quantity;price;commission");
  for(std::size_t i=0;i<6;++i){const auto h=upper(trim(rows[0].cells[i]));const auto it=std::find(names.begin(),names.end(),h);if(it==names.end()||!headers.insert(h).second)throw std::runtime_error("Неизвестный или повторный заголовок CSV");columns[static_cast<std::size_t>(it-names.begin())]=i;}
  std::set<Key> seen;for(const auto& t:existing)seen.insert(key(t));
  for(std::size_t i=1;i<rows.size();++i){
   ImportRow row;row.line=rows[i].line;
   if(rows[i].cells.size()!=6){row.message="Должно быть 6 полей";result.rows.push_back(row);continue;}
   const auto& cells=rows[i].cells;
   const auto date=trim(cells[columns[0]]),ticker=upper(trim(cells[columns[1]])),side=upper(trim(cells[columns[2]]));
   const auto q=number(cells[columns[3]]),p=number(cells[columns[4]]),fee=number(cells[columns[5]]);
   if(side!="BUY"&&side!="SELL"){row.message="side: допустимы BUY или SELL";result.rows.push_back(row);continue;}
   if(!q||!p||!fee){row.message="Некорректное число или пустое поле; комиссия обязательна (можно 0)";result.rows.push_back(row);continue;}
   PortfolioTransaction t{date,ticker,side=="BUY"?PortfolioSide::Buy:PortfolioSide::Sell,*q,*p,*fee};
   if(!valid_portfolio_transaction(t)){row.message="Проверьте дату YYYY-MM-DD, тикер, положительные количество/цену и комиссию >= 0";result.rows.push_back(row);continue;}
   row.transaction=t;const bool duplicate=!seen.insert(key(t)).second;
   row.status=duplicate?ImportStatus::Duplicate:ImportStatus::New;
   row.message=duplicate?"Совпадает с операцией в журнале или предыдущей строкой файла":"Новая операция";
   result.rows.push_back(std::move(row));
  }
  if(result.rows.empty())result.error="В файле нет операций";
 }catch(const std::exception& ex){result.error=ex.what();}
 return result;
}
std::optional<std::vector<PortfolioTransaction>> prepare_portfolio_import(const ImportPreview& preview,
 const std::vector<PortfolioTransaction>& existing,bool include_duplicates,std::string& error){
 error.clear();if(!preview.error.empty()){error=preview.error;return {};}
 auto result=existing;std::size_t added=0;
 for(const auto& row:preview.rows){
  if(row.status==ImportStatus::Error||!row.transaction){error="Строка "+std::to_string(row.line)+": "+row.message+". Импорт целиком заблокирован.";return {};}
  if(row.status==ImportStatus::Duplicate&&!include_duplicates)continue;
  result.push_back(*row.transaction);++added;
 }
 if(added==0){error="Нет новых операций. Совпадения по умолчанию пропущены.";return {};}
 std::stable_sort(result.begin(),result.end(),[](const auto&a,const auto&b){return a.date<b.date;});
 if(!calculate_portfolio(result,{})){error="После импорта продажа превышает остаток либо расчёт невозможен. При одинаковой дате журнал идёт перед CSV, внутри CSV сохраняется порядок строк.";return {};}
 return result;
}
std::string export_portfolio_csv(const std::vector<PortfolioTransaction>& transactions){
 std::ostringstream out;out<<"\xEF\xBB\xBF"<<"date;ticker;side;quantity;price;commission\r\n"<<std::setprecision(17);
 for(const auto&t:transactions){if(!valid_portfolio_transaction(t))throw std::runtime_error("Журнал содержит некорректную операцию");out<<t.date<<';'<<t.ticker<<';'<<(t.side==PortfolioSide::Buy?"BUY":"SELL")<<';'<<t.quantity<<';'<<t.price<<';'<<t.commission<<"\r\n";}
 return out.str();
}
}
