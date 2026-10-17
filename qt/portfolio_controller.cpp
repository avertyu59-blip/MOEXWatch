#include "portfolio_controller.hpp"
#include "market_data.hpp"
#include "observation.hpp"
#include "storage.hpp"
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QLocale>
#include <QUrl>
#include <algorithm>
#include <charconv>
#include <cmath>

using namespace moexwatch;
namespace {
QString text(std::string_view s) { return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())); }
QString number(double n, int digits = 2) { return QLocale(QLocale::Russian).toString(n, 'f', digits); }
QString optional(std::optional<double> n, QString suffix = {}) { return n ? number(*n) + suffix : QStringLiteral("—"); }
std::optional<double> parse(QString value) {
    auto bytes=value.trimmed().replace(',', '.').toStdString();
    double result{};const auto [end,error]=std::from_chars(bytes.data(),bytes.data()+bytes.size(),result);
    if(error!=std::errc{}||end!=bytes.data()+bytes.size()||!std::isfinite(result))return {};
    return result;
}
std::filesystem::path nativePath(const QString& value) {
#ifdef _WIN32
    return value.toStdWString();
#else
    return value.toStdString();
#endif
}
QString pathText(const std::filesystem::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}
}
int RecordModel::rowCount(const QModelIndex& parent) const { return parent.isValid()?0:static_cast<int>(rows_.size()); }
QVariant RecordModel::data(const QModelIndex& index,int role) const {
    if(index.isValid()&&index.column()==0&&index.row()>=0&&index.row()<rows_.size()&&role==Qt::UserRole+1)return rows_[index.row()];
    return {};
}
QHash<int,QByteArray> RecordModel::roleNames() const { return {{Qt::UserRole+1,"record"}}; }
void RecordModel::replace(QList<QVariantMap> rows) { beginResetModel();rows_=std::move(rows);endResetModel(); }

PortfolioController::PortfolioController(std::filesystem::path root,Fetch fetch,QObject* parent)
    :QObject(parent),storage_(std::move(root)),fetch_(std::move(fetch)) {
    try {
        const auto dir=pathText(storage_.root());
        if(!QDir().mkpath(dir))throw std::runtime_error("Не удалось создать каталог данных");
        lock_=std::make_unique<QLockFile>(dir+"/qt-session.lock");
        if(!lock_->tryLock(0))throw std::runtime_error("Другая Qt-копия уже использует этот профиль");
        // Settings failure must never disable financial journal writes.
        settings_=std::make_unique<QSettings>(dir+"/qt-workspace.ini",QSettings::IniFormat);
        dark_=settings_->value("appearance/dark",true).toBool();
        journal_=storage_.load_portfolio();
        instruments_=storage_.load_watchlist();
        status_="Данные загружены. Обновите котировки для оценки позиций.";
    } catch(const std::exception& ex) { fail("Не удалось загрузить профиль: "+text(ex.what()),true); }
    if(!fetch_) {
        const auto cache=storage_.root();
        fetch_=[cache](std::vector<Instrument> items,std::stop_token stop) {
            MarketRefreshResult result;
#ifdef _WIN32
            FetchContext context{stop,winhttp_get,std::make_shared<DiagnosticLog>(cache/"logs"/"diagnostic.log"),0,cache};
            for(const auto& item:items){check_cancelled(stop);result.instruments.push_back(fetch_instrument(item,context));}
#else
            (void)cache;(void)items;(void)stop;
            throw std::runtime_error("Сетевой адаптер этого preview доступен в Windows-сборке");
#endif
            return result;
        };
    }
    connect(&timer_,&QTimer::timeout,this,&PortfolioController::drain);timer_.start(50);
    rebuild();
}
PortfolioController::~PortfolioController(){shutdown();}
void PortfolioController::shutdown(){if(closed_)return;closed_=true;timer_.stop();mailbox_->close();tasks_.shutdown();active_request_=0;}
void PortfolioController::fail(QString message,bool blockWrites){error_=std::move(message);if(blockWrites)writable_=false;emit changed();}
void PortfolioController::clearError(){error_.clear();emit changed();}
void PortfolioController::setDark(bool value) {
    dark_=value;
    if(settings_) {
        settings_->setValue("appearance/dark",value);settings_->sync();
        if(settings_->status()!=QSettings::NoError)error_="Тема изменена только для этой сессии: настройки недоступны для записи.";
    }
    emit changed();
}
QStringList PortfolioController::tickers() const {
    QStringList values;for(const auto& i:instruments_)values.push_back(text(i.ticker));
    for(const auto& t:journal_.transactions)values.push_back(text(t.ticker));
    values.removeDuplicates();values.sort();return values;
}
bool PortfolioController::persist(PortfolioJournal candidate) {
    if(!writable_){fail("Запись заблокирована. Сначала восстановите профиль из резервной копии.");return false;}
    try{storage_.save_portfolio(candidate);journal_=std::move(candidate);error_.clear();rebuild();return true;}
    catch(const std::exception& ex){fail("Журнал не изменён: "+text(ex.what()),true);return false;}
}
bool PortfolioController::addOperation(QString date,QString ticker,int side,QString quantity,QString price,QString fee) {
    ticker=ticker.trimmed().toUpper();date=date.trimmed();
    const auto q=parse(quantity),p=parse(price),f=fee.trimmed().isEmpty()?std::optional<double>{0}:parse(fee);
    if(side<0||side>1||!q||!p||!f||QDate::fromString(date,Qt::ISODate).toString(Qt::ISODate)!=date) {
        fail("Проверьте дату ГГГГ-ММ-ДД, сторону и числовые значения.");return false;
    }
    PortfolioTransaction operation{date.toStdString(),ticker.toStdString(),side==0?PortfolioSide::Buy:PortfolioSide::Sell,*q,*p,*f};
    if(!valid_portfolio_transaction(operation)){fail("Тикер некорректен, количество/цена не положительны или комиссия отрицательна.");return false;}
    auto next=journal_;next.transactions.push_back(operation);
    try {
        next.transactions=identify_portfolio(std::move(next.transactions),next.sequence_watermark);
        for(const auto& t:next.transactions)next.sequence_watermark=std::max(next.sequence_watermark,t.sequence);
    }catch(const std::exception& ex){fail(text(ex.what()));return false;}
    return persist(std::move(next));
}
bool PortfolioController::deleteOperation(QString id) {
    auto next=journal_;const auto found=std::find_if(next.transactions.begin(),next.transactions.end(),[&](const auto& t){return text(t.id)==id;});
    if(found==next.transactions.end()){fail("Операция уже отсутствует. Обновите выбор.");return false;}
    next.transactions.erase(found);
    try {next.transactions=identify_portfolio(std::move(next.transactions),next.sequence_watermark);}
    catch(const std::exception& ex){fail("Удаление нарушает историю позиции: "+text(ex.what()));return false;}
    return persist(std::move(next));
}
QVariantMap PortfolioController::position(QString id) const {
    for(const auto& row:positions_.rows())if(row.value("id").toString()==id)return row;
    return {};
}
bool PortfolioController::saveLevels(QString id,QString stop,QString take) {
    const auto row=position(id);if(row.isEmpty()){fail("Открытая позиция уже отсутствует.");return false;}
    const auto s=stop.trimmed().isEmpty()?std::optional<double>{}:parse(stop);
    const auto t=take.trimmed().isEmpty()?std::optional<double>{}:parse(take);
    if((!stop.trimmed().isEmpty()&&!s)||(!take.trimmed().isEmpty()&&!t)){fail("Введите положительные уровни или оставьте поля пустыми.");return false;}
    PositionLevels level{row.value("ticker").toString().toStdString(),row.value("opened").toString().toStdString(),s,t,id.toStdString()};
    if(!valid_position_levels(level)){fail("Проверьте стоп-лосс и тейк-профит.");return false;}
    auto next=journal_;std::erase_if(next.levels,[&](const auto& l){return l.position_id==level.position_id;});next.levels.push_back(level);
    return persist(std::move(next));
}
void PortfolioController::sortPositions(QString key) {
    if(key!="ticker"&&key!="weight"&&key!="profit")return;
    sort_key_=std::move(key);rebuild();
}
void PortfolioController::rebuild() {
    std::vector<Quote> quotes;for(const auto& i:instruments_)quotes.push_back(i.quote);
    const auto result=calculate_portfolio(journal_.transactions,quotes);
    QList<QVariantMap> rows,operations;allocation_.clear();
    const auto now=QDateTime::currentSecsSinceEpoch();
    bool fresh=static_cast<bool>(result);
    if(result)for(const auto& p:result->positions)if(p.quantity>0) {
        const auto i=std::find_if(instruments_.begin(),instruments_.end(),[&](const auto& item){return item.ticker==p.ticker;});
        if(i==instruments_.end()||i->quote_stale||!fresh_moex_quote(i->quote,now)||!p.market_value)fresh=false;
    }
    const bool complete=result&&result->open_positions==result->priced_positions;
    if(result)for(const auto& p:result->positions)if(p.quantity>0) {
        const auto i=std::find_if(instruments_.begin(),instruments_.end(),[&](const auto& item){return item.ticker==p.ticker;});
        const bool stale=i==instruments_.end()||i->quote_stale||!fresh_moex_quote(i->quote,now);
        auto level=std::find_if(journal_.levels.begin(),journal_.levels.end(),[&](const auto& l){return l.position_id==p.position_id;});
        const auto weight=portfolio_weight(*result,p,fresh);
        rows.push_back({{"id",text(p.position_id)},{"ticker",text(p.ticker)},{"opened",text(p.opened_on)},
            {"quantity",number(p.quantity, p.quantity==std::floor(p.quantity)?0:4)},{"average",number(p.average_entry_price)},
            {"price",optional(p.current_price)},{"value",optional(p.market_value)},{"profit",optional(p.unrealized_profit)},
            {"percent",optional(p.unrealized_percent,"%")},{"profitValue",p.unrealized_profit.value_or(0)},
            {"weightValue",weight.value_or(0)},{"weight",optional(weight,"%")},{"stale",stale},
            {"stop",level!=journal_.levels.end()&&level->stop_loss?QString::number(*level->stop_loss,'g',15):QString{}},
            {"take",level!=journal_.levels.end()&&level->take_profit?QString::number(*level->take_profit,'g',15):QString{}}});
    }
    std::stable_sort(rows.begin(),rows.end(),[&](const auto& a,const auto& b){
        if(sort_key_=="ticker")return a.value("ticker").toString()<b.value("ticker").toString();
        const auto key=sort_key_=="weight"?"weightValue":"profitValue";
        return a.value(key).toDouble()>b.value(key).toDouble();
    });
    if(fresh&&result&&result->total_market_value>0)for(const auto& row:rows)
        allocation_.push_back(QVariantMap{{"ticker",row.value("ticker")},{"weight",row.value("weightValue")},{"value",row.value("value")}});
    positions_.replace(std::move(rows));
    auto ordered=journal_.transactions;
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.date!=b.date?a.date>b.date:a.sequence>b.sequence;});
    for(const auto& t:ordered)operations.push_back({{"id",text(t.id)},{"positionId",text(t.position_id)},{"date",text(t.date)},
        {"ticker",text(t.ticker)},{"side",t.side==PortfolioSide::Buy?"Покупка":"Продажа"},
        {"quantity",number(t.quantity,4)},{"price",number(t.price)},{"fee",number(t.commission)}});
    operations_.replace(std::move(operations));
    totals_={{"value",result&&complete?number(result->total_market_value)+" ₽":"—"},
        {"unrealized",result&&complete?number(result->total_unrealized_profit)+" ₽":"—"},
        {"realized",result?number(result->total_realized_profit)+" ₽":"—"},
        {"unrealizedValue",result?result->total_unrealized_profit:0},{"realizedValue",result?result->total_realized_profit:0},
        {"fresh",fresh},{"complete",complete},{"openCount",result?static_cast<int>(result->open_positions):0},
        {"valuationNote",fresh?"По актуальным котировкам":complete?"Последняя оценка · котировки устарели":"Нет котировок всех позиций"}};
    emit changed();
}
void PortfolioController::refresh() {
    if(closed_||busy()||tasks_.running(TaskKind::Market))return;
    auto input=instruments_;
    for(const auto& ticker:tickers())if(std::none_of(input.begin(),input.end(),[&](const auto& i){return text(i.ticker)==ticker;})) {
        Instrument i;i.ticker=ticker.toStdString();i.quote.ticker=i.ticker;input.push_back(std::move(i));
    }
    if(input.empty()){status_="Сначала запишите операцию или добавьте бумаги в классической версии.";emit changed();return;}
    const auto request=++request_sequence_;active_request_=request;
    const auto fetch=fetch_;status_="Обновление котировок MOEX…";
    try{tasks_.start(TaskKind::Market,mailbox_,[input=std::move(input),fetch,request](std::stop_token stop)mutable->BackgroundEvent {
        try {auto result=fetch(std::move(input),stop);check_cancelled(stop);return {request,std::move(result)};}
        catch(const Cancelled&){return {request,TaskFailure{TaskKind::Market,true}};}
        catch(...){return {request,TaskFailure{TaskKind::Market,false}};}
    });}catch(const std::exception& ex){active_request_=0;fail(text(ex.what()));}
    emit changed();
}
void PortfolioController::cancel(){if(!busy())return;tasks_.cancel(TaskKind::Market);active_request_=0;status_="Загрузка отменена; предыдущие данные сохранены.";emit changed();}
void PortfolioController::backup(){
    if(closed_||backupBusy())return;
    const auto storage=storage_;
    try{tasks_.start(TaskKind::Backup,mailbox_,[storage](std::stop_token stop)->BackgroundEvent {
        BackupResult result;try{result.path=storage.create_backup(stop);}catch(const Cancelled&){result.cancelled=true;}catch(const std::exception& ex){result.error=ex.what();}return {0,std::move(result)};
    });}catch(const std::exception& ex){fail(text(ex.what()));}emit changed();
}
void PortfolioController::openDataFolder(){QDesktopServices::openUrl(QUrl::fromLocalFile(pathText(storage_.root())));}
void PortfolioController::drain(){
    for(auto& event:mailbox_->take()) {
        if(auto* result=std::get_if<MarketRefreshResult>(&event.data)) {
            if(event.request!=active_request_||active_request_==0)continue;
            active_request_=0;
            if(result->cancelled){status_="Загрузка отменена.";emit changed();continue;}
            // Only replace quote/history fields: concurrent portfolio edits remain UI-owned.
            for(auto& incoming:result->instruments) {
                const auto i=std::find_if(instruments_.begin(),instruments_.end(),[&](const auto& old){return old.ticker==incoming.ticker;});
                if(i==instruments_.end())instruments_.push_back(std::move(incoming));else *i=std::move(incoming);
            }
            const auto fresh=std::count_if(instruments_.begin(),instruments_.end(),[](const auto& i){return !i.quote_stale&&i.quote.last;});
            status_="Котировки: "+QString::number(fresh)+" / "+QString::number(instruments_.size())+" · проверка "+QTime::currentTime().toString("HH:mm:ss");
            if(fresh==0)error_="MOEX не вернул актуальные котировки. История и операции сохранены.";
            rebuild();
        }else if(auto* result=std::get_if<BackupResult>(&event.data)) {
            if(!result->error.empty())fail("Резервная копия: "+text(result->error));
            else if(!result->cancelled)status_="Копия создана: "+pathText(result->path);
            emit changed();
        }else if(auto* failure=std::get_if<TaskFailure>(&event.data)) {
            if(failure->kind==TaskKind::Market&&active_request_!=0&&event.request==active_request_) {
                active_request_=0;status_="Обновление не завершено; предыдущие данные сохранены.";
                if(!failure->cancelled)error_="Источник недоступен или загрузка завершилась ошибкой. Подробности — в журнале диагностики.";
            }
            emit changed();
        }
    }
}
void PortfolioController::loadDemo(){
    cancel();tasks_.shutdown();mailbox_->take();writable_=false;error_.clear();journal_={};instruments_.clear();
    const char* names[]={"SBER","GAZP","LKOH","YDEX"};
    const double entry[]={265.4,153.7,6410,3520},current[]={282.15,146.32,6845,3560},quantity[]={500,400,100,80};
    const auto stamp=observation_time(QDateTime::currentSecsSinceEpoch());
    for(int i=0;i<4;++i){journal_.transactions.push_back({"2026-10-01",names[i],PortfolioSide::Buy,quantity[i],entry[i],0});
        Instrument item;item.ticker=names[i];item.quote={names[i],current[i],0,stamp.substr(11),stamp};item.quote_stale=false;instruments_.push_back(std::move(item));}
    journal_.transactions=identify_portfolio(std::move(journal_.transactions));
    for(const auto& t:journal_.transactions)journal_.sequence_watermark=std::max(journal_.sequence_watermark,t.sequence);
    status_="ДЕМОНСТРАЦИЯ · вымышленные операции и котировки · запись отключена";rebuild();
}
