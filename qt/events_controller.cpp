#include "events_controller.hpp"
#include "market_data.hpp"
#include "observation.hpp"
#include "storage.hpp"
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <cmath>
using namespace moexwatch;
namespace {
QString q(std::string_view s) {
  return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}
std::filesystem::path cache(const std::filesystem::path &root,
                            const QString &ticker) {
  if (!valid_ticker(ticker.toStdString()))
    throw std::runtime_error("Некорректный тикер");
  return root / ("dividends_" + ticker.toStdString() + ".csv");
}
QString eventStatus(DividendStatus s) {
  switch (s) {
  case DividendStatus::Forecast:
    return "Прогноз";
  case DividendStatus::Recommended:
    return "Рекомендовано";
  case DividendStatus::Approved:
    return "Утверждено";
  case DividendStatus::Cancelled:
    return "Отменено";
  default:
    return "Статус не подтверждён";
  }
}
QString verification(DividendVerification s) {
  switch (s) {
  case DividendVerification::Matched:
    return "Совпадение с эмитентом";
  case DividendVerification::Conflict:
    return "Расхождение с эмитентом";
  case DividendVerification::Unavailable:
    return "Сверка недоступна";
  default:
    return "Не сверено с эмитентом";
  }
}
} // namespace
EventsController::EventsController(PortfolioController *market, QObject *parent)
    : EventsController(market, Fetch{}, parent) {}
EventsController::EventsController(PortfolioController *market, Fetch fetch,
                                   QObject *parent)
    : QObject(parent), market_(market), fetch_(std::move(fetch)) {
  if (!fetch_) {
#ifdef _WIN32
    const auto root = market_->dataRoot();
    fetch_ = [root](std::string ticker, std::stop_token stop) {
      DividendFetchResult result;
      result.ticker = std::move(ticker);
      result.checked_at =
          QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
      try {
        FetchContext context{
            stop, winhttp_get,
            std::make_shared<DiagnosticLog>(root / "network.log"), 0, root};
        fetch_dividend_events(result, context);
      } catch (const Cancelled &) {
        result.cancelled = true;
      } catch (const std::exception &ex) {
        result.error = ex.what();
      }
      return result;
    };
#else
    fetch_ = [](std::string ticker, std::stop_token) {
      DividendFetchResult r;
      r.ticker = std::move(ticker);
      r.error = "Сетевая загрузка доступна в сборке Windows";
      return r;
    };
#endif
  }
  connect(market_, &PortfolioController::selectionChanged, this,
          &EventsController::select);
  connect(market_, &PortfolioController::marketDataChanged, this,
          &EventsController::rebuild);
  connect(market_, &PortfolioController::journalChanged, this, &EventsController::rebuild);
  connect(market_, &PortfolioController::writableChanged, this, &EventsController::changed);
  connect(&timer_, &QTimer::timeout, this, &EventsController::drain);
  timer_.start(30);
  loadCorporate();
  select();
}
EventsController::~EventsController() { shutdown(); }
void EventsController::shutdown() {
  if (closed_)
    return;
  closed_ = true;
  timer_.stop();
  mailbox_->close();
  tasks_.shutdown();
  active_ = 0;
}
void EventsController::select() {
  cancel();
  ticker_ = market_->selectedTicker();
  snapshot_ = {ticker_.toStdString(), {}, {}};
  stale_ = true;
  error_.clear();
  status_ = "Нет загруженных событий. Нажмите «Обновить»";
  if (!ticker_.isEmpty() && !market_->demo())
    try {
      if (const auto bytes = storage::read_text(
              cache(market_->dataRoot(), ticker_), 1024U * 1024U)) {
        const auto saved =
            parse_dividend_snapshot(*bytes, ticker_.toStdString());
        if (!saved)
          throw std::runtime_error("Кэш дивидендов повреждён; обновите данные");
        snapshot_ = *saved;
        status_ = "Сохранённые данные · " + q(snapshot_.checked_at) +
                  " · требуется обновление";
      }
    } catch (const std::exception &ex) {
      error_ = q(ex.what());
    }
  rebuild();
  emit snapshotChanged();
}
void EventsController::cancel() {
  const bool wasBusy = busy();
  tasks_.cancel(TaskKind::Dividends);
  active_ = 0;
  if (wasBusy)
    status_ = "Загрузка отменена · сохранённые события не изменены";
  if (!closed_)
    emit changed();
}
void EventsController::refresh() {
  if (closed_ || busy() || tasks_.running(TaskKind::Dividends) ||
      ticker_.isEmpty())
    return;
  if (market_->demo()) {
    status_ =
        "Демонстрация: сетевые события не смешиваются с вымышленным портфелем";
    emit changed();
    return;
  }
  const auto request = ++sequence_;
  const auto ticker = ticker_.toStdString();
  const auto fetch = fetch_;
  active_ = request;
  error_.clear();
  status_ = "Загрузка открытого календаря и проверка источников…";
  if (!tasks_.start(TaskKind::Dividends, mailbox_,
                    [fetch, ticker, request](std::stop_token stop) {
                      DividendFetchResult r;
                      try {
                        r = fetch(ticker, stop);
                        if (stop.stop_requested())
                          r.cancelled = true;
                      } catch (const Cancelled &) {
                        r.cancelled = true;
                      } catch (const std::exception &ex) {
                        r.error = ex.what();
                      }
                      return BackgroundEvent{request, std::move(r)};
                    })) {
    active_ = 0;
    error_ = "Загрузка ещё завершается; повторите обновление";
  }
  emit changed();
}
void EventsController::drain() {
  for (auto &event : mailbox_->take()) {
    if (event.request != active_)
      continue;
    active_ = 0;
    const auto *result = std::get_if<DividendFetchResult>(&event.data);
    if (!result) {
      error_ = "Загрузка прервана";
      emit changed();
      continue;
    }
    if (result->cancelled || q(result->ticker) != ticker_) {
      status_ = "Загрузка отменена";
      emit changed();
      continue;
    }
    if (!result->error.empty()) {
      error_ = q(result->error);
      stale_ = true;
      status_ = "Ошибка источника · сохранённые данные не подтверждены";
      rebuild();
      continue;
    }
    // Round-trip the same strict snapshot validator used for disk caches before
    // applying.
    const DividendSnapshot candidate{result->ticker, result->checked_at,
                                     result->events};
    const auto serialized = serialize_dividend_snapshot(candidate);
    const auto valid = parse_dividend_snapshot(serialized, result->ticker);
    if (!valid) {
      error_ = "Источник вернул некорректные события";
      stale_ = true;
      rebuild();
      continue;
    }
    snapshot_ = *valid;
    stale_ = false;
    status_ = "Проверено " + q(snapshot_.checked_at) + " · " + q(result->note);
    try {
      storage::atomic_write(cache(market_->dataRoot(), ticker_), serialized);
    } catch (const std::exception &ex) {
      error_ = "Не удалось сохранить кэш: " + q(ex.what());
    }
    rebuild();
    emit snapshotChanged();
  }
}
void EventsController::rebuild() {
  QList<QVariantMap> corporateRows;
  for (const auto &row : corporate_records_)
    if (row["ticker"].toString() == ticker_ &&
        (!only_positions_ || availableTickers().contains(ticker_)))
      corporateRows.push_back(row);
  if (corporateRows != corporate_.rows())
    corporate_.replace(corporateRows);
  if (!calendar_error_.isEmpty())
    error_ = calendar_error_;
  std::optional<double> price;
  for (const auto &item : market_->instrumentsSnapshot())
    if (item.ticker == ticker_.toStdString() && !item.quote_stale &&
        fresh_moex_quote(item.quote, QDateTime::currentSecsSinceEpoch()))
      price = item.quote.last;
  QList<QVariantMap> rows;
  for (const auto &e : snapshot_.events) {
    if (only_positions_ && !availableTickers().contains(ticker_))
      continue;
    QVariantMap row{{"key", ticker_+"|"+q(e.record_date)+"|"+q(e.period)+"|"+q(e.currency)},
                    {"recordDate", q(e.record_date)},
                    {"lastBuy", q(e.last_buy_date)},
                    {"lastBuyBasis", q(e.last_buy_basis)},
                    {"amount", e.amount_per_share},
                    {"currency", q(e.currency)},
                    {"status", eventStatus(e.status)},
                    {"verification", verification(e.verification)},
                    {"source", q(e.source_url)},
                    {"issuerSource", q(e.issuer_url)},
                    {"issuerChecked", q(e.issuer_checked_at)},
                    {"period", q(e.period)},
                    {"stale", stale_}};
    if (price && *price > 0 && e.currency == "RUB" &&
        e.status != DividendStatus::Cancelled)
      row["yield"] = 100 * e.amount_per_share / *price;
    rows.push_back(row);
  }
  if (rows != rows_.rows())
    rows_.replace(std::move(rows));
  emit changed();
}
void EventsController::openSource(QString url) {
  const QUrl parsed(url);
  if (parsed.isValid() && parsed.scheme() == "https" &&
      !parsed.host().isEmpty() && parsed.userInfo().isEmpty())
    QDesktopServices::openUrl(parsed);
}

bool EventsController::calendarWritable() const {
  return calendar_valid_ && market_->writable() && !market_->demo();
}
void EventsController::loadCorporate() {
  try {
    const auto bytes = storage::read_text(
        market_->dataRoot() / "corporate_events_v1.json", 1024U * 1024U);
    if (!bytes)
      return;
    QJsonParseError parseError;
    const auto doc =
        QJsonDocument::fromJson(QByteArray::fromStdString(*bytes), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject() ||
        doc.object()["version"].toInt() != 1 ||
        !doc.object()["events"].isArray())
      throw std::runtime_error(
          "Повреждён календарь событий; запись заблокирована");
    QSet<QString> ids;
    QList<QVariantMap> candidate;
    for (const auto v : doc.object()["events"].toArray()) {
      if (!v.isObject())
        throw std::runtime_error("Некорректное событие календаря");
      const auto r = v.toObject().toVariantMap();
      const auto id = r["id"].toString();
      const auto date = r["date"].toString();
      const auto url = QUrl(r["source"].toString());
      if (QUuid(id).isNull() || ids.contains(id) ||
          !valid_ticker(r["ticker"].toString().toStdString()) ||
          QDate::fromString(date, Qt::ISODate).toString(Qt::ISODate) != date ||
          !QStringList{"Отчётность", "Собрание", "Другое"}.contains(
              r["type"].toString()) ||
          r["title"].toString().trimmed().isEmpty() ||
          r["title"].toString().size() > 300 ||
          !QStringList{"Запланировано", "Подтверждено", "Отменено"}.contains(
              r["status"].toString()) ||
          !url.isValid() || url.scheme() != "https" || url.host().isEmpty() ||
          !url.userInfo().isEmpty())
        throw std::runtime_error("Некорректное событие календаря");
      ids.insert(id);
      candidate.push_back(r);
    }
    corporate_records_ = std::move(candidate);
  } catch (const std::exception &ex) {
    calendar_valid_ = false;
    calendar_error_ = q(ex.what());
  }
}
bool EventsController::saveCorporate(const QList<QVariantMap> &candidate) {
  if (!calendarWritable()) {
    error_ = "Запись календаря недоступна";
    emit changed();
    return false;
  }
  try {
    QJsonArray array;
    for (const auto &row : candidate)
      array.append(QJsonObject::fromVariantMap(row));
    const auto bytes =
        QJsonDocument(QJsonObject{{"version", 1}, {"events", array}}).toJson();
    if (bytes.size() > 1024 * 1024)
      throw std::runtime_error("Календарь превышает допустимый размер");
    storage::atomic_write(market_->dataRoot() / "corporate_events_v1.json",
                          bytes.toStdString());
    corporate_records_ = candidate;
    rebuild();
    return true;
  } catch (const std::exception &ex) {
    error_ = q(ex.what());
    emit changed();
    return false;
  }
}
bool EventsController::addCorporate(QString date, QString type, QString title,
                                    QString source, QString status) {
  title = title.trimmed();
  source = source.trimmed();
  const QUrl url(source);
  if (ticker_.isEmpty() ||
      QDate::fromString(date, Qt::ISODate).toString(Qt::ISODate) != date ||
      !QStringList{"Отчётность", "Собрание", "Другое"}.contains(type) ||
      title.isEmpty() || title.size() > 300 ||
      !QStringList{"Запланировано", "Подтверждено", "Отменено"}.contains(
          status) ||
      !url.isValid() || url.scheme() != "https" || url.host().isEmpty() ||
      !url.userInfo().isEmpty()) {
    error_ =
        "Проверьте дату YYYY-MM-DD, тип, описание и HTTPS-источник события";
    emit changed();
    return false;
  }
  auto candidate = corporate_records_;
  candidate.push_back(
      {{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
       {"ticker", ticker_},
       {"date", date},
       {"type", type},
       {"title", title},
       {"source", source},
       {"status", status}});
  return saveCorporate(candidate);
}
bool EventsController::removeCorporate(QString id) {
  auto candidate = corporate_records_;
  const auto old = candidate.size();
  candidate.erase(std::remove_if(candidate.begin(), candidate.end(),
                                 [&](const auto &row) {
                                   return row["id"].toString() == id;
                                 }),
                  candidate.end());
  return old != candidate.size() && saveCorporate(candidate);
}

QStringList EventsController::availableTickers() const {
  QStringList result;
  if (only_positions_) {
    for (const auto &r : market_->positions()->rows())
      result.push_back(r["ticker"].toString());
  } else {
    result = market_->tickers();
    for (const auto &t : market_->journalSnapshot().transactions)
      result.push_back(q(t.ticker));
  }
  result.removeDuplicates();
  result.sort();
  return result;
}
void EventsController::setOnlyPositions(bool value) {
  if (value == only_positions_)
    return;
  only_positions_ = value;
  auto names = availableTickers();
  if (!names.contains(market_->selectedTicker()) && !names.isEmpty())
    market_->setSelectedTicker(names.first());
  rebuild();
}
