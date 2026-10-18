#include "portfolio_dividends_controller.hpp"
#include "market_core.hpp"
#include "storage.hpp"
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTimeZone>
#include <QUrl>
#include <QUuid>
#include <cmath>
using namespace moexwatch;
namespace {
QString q(std::string_view s) {
  return QString::fromUtf8(s.data(), qsizetype(s.size()));
}
QString money(double d) {
  return QString::number(d, 'f', 2) + QStringLiteral(" ₽");
}
bool date(QString s) {
  auto d = QDate::fromString(s, Qt::ISODate);
  return d.isValid() && d.year() >= 1990 && d.year() <= 2100 &&
         d.toString(Qt::ISODate) == s;
}
QString todayMoscow() {
  return QDateTime::currentDateTimeUtc()
      .toTimeZone(QTimeZone::fromSecondsAheadOfUtc(10800))
      .date()
      .toString(Qt::ISODate);
}
bool number(const QVariantMap &r, const char *k, double lo, double hi) {
  auto v = r.value(k);
  return (v.metaType().id() == QMetaType::Double ||
          v.metaType().id() == QMetaType::LongLong ||
          v.metaType().id() == QMetaType::Int) &&
         std::isfinite(v.toDouble()) && v.toDouble() >= lo &&
         v.toDouble() <= hi;
}
QString fingerprint(const QVariantMap &map) {
  return QString::fromLatin1(
      QCryptographicHash::hash(QJsonDocument(QJsonObject::fromVariantMap(map))
                                   .toJson(QJsonDocument::Compact),
                               QCryptographicHash::Sha256)
          .toHex());
}
std::vector<DividendPositionEntitlement> positions(const QVariantList &rows) {
  std::vector<DividendPositionEntitlement> result;
  for (const auto &v : rows) {
    auto p = v.toMap();
    result.push_back(
        {p["id"].toString().toStdString(), p["quantity"].toDouble(), 0});
  }
  return result;
}
bool validReceipt(const QVariantMap &r) {
  const auto id = r.value("id").toString(),
             ticker = r.value("ticker").toString(),
             record = r.value("recordDate").toString(),
             cutoff = r.value("cutoff").toString();
  const QUrl source(r.value("source").toString());
  const auto expectedCutoff =
      dividend_last_buy(record.toStdString(), moex_calendar_2026());
  if (QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces) != id ||
      !valid_ticker(ticker.toStdString()) || ticker != ticker.toUpper() ||
      !date(record) || !date(cutoff) || !expectedCutoff ||
      q(*expectedCutoff) != cutoff ||
      !date(r.value("paymentDate").toString()) ||
      r.value("paymentDate").toString() < record ||
      r.value("paymentDate").toString() > todayMoscow() ||
      r.value("period").metaType().id() != QMetaType::QString ||
      r.value("period").toString().size() > 300 ||
      r.value("key").toString() !=
          ticker + "|" + record + "|" + r.value("period").toString() ||
      !number(r, "status", 3, 3) || !number(r, "verification", 1, 1) ||
      !number(r, "quantity", 1e-9, 1e12) ||
      !number(r, "perShare", 1e-9, 1e12) || !number(r, "gross", 1e-9, 1e15) ||
      !number(r, "tax", 0, 1e15) ||
      r.value("tax").toDouble() > r.value("gross").toDouble() ||
      r.value("reference").metaType().id() != QMetaType::QString ||
      r.value("reference").toString().trimmed().isEmpty() ||
      r.value("reference").toString().size() > 1000 ||
      r.value("positions").metaType().id() != QMetaType::QVariantList ||
      !source.isValid() || source.scheme() != "https" ||
      source.host().isEmpty() || !source.userInfo().isEmpty() ||
      r.value("source").toString().size() > 2048 ||
      !QDateTime::fromString(r.value("recordedAt").toString(), Qt::ISODate)
           .isValid() ||
      !date(r.value("checkedAt").toString().left(10)) ||
      r.value("journalRevision").metaType().id() != QMetaType::QString)
    return false;
  bool revisionOk = false;
  r.value("journalRevision").toString().toULongLong(&revisionOk);
  if (!revisionOk)
    return false;
  QSet<QString> ids;
  double quantity = 0;
  for (const auto &v : r.value("positions").toList()) {
    if (v.metaType().id() != QMetaType::QVariantMap)
      return false;
    auto p = v.toMap();
    auto pid = p.value("id").toString();
    if (QUuid(pid).isNull() ||
        QUuid(pid).toString(QUuid::WithoutBraces) != pid || ids.contains(pid) ||
        !number(p, "quantity", 1e-9, 1e12))
      return false;
    ids.insert(pid);
    quantity += p.value("quantity").toDouble();
  }
  return std::isfinite(quantity) &&
         std::abs(quantity - r.value("quantity").toDouble()) <=
             1e-8 * std::max(1., quantity) &&
         allocate_dividend_income(positions(r.value("positions").toList()),
                                  r.value("gross").toDouble(),
                                  r.value("tax").toDouble())
             .has_value();
}
} // namespace
PortfolioDividendsController::PortfolioDividendsController(
    PortfolioController *m, QObject *p)
    : QObject(p), market_(m) {
  connect(m, &PortfolioController::changed, this,
          &PortfolioDividendsController::reload);
  load();
  rebuild();
}
void PortfolioDividendsController::load() {
  try {
    const auto bytes = storage::read_text(
        market_->dataRoot() / "portfolio_dividend_receipts_v1.json",
        2 * 1024 * 1024);
    if (!bytes)
      return;
    persisted_hash_ = QCryptographicHash::hash(
        QByteArray::fromStdString(*bytes), QCryptographicHash::Sha256);
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(*bytes), &e);
    auto root = doc.object();
    if (e.error != QJsonParseError::NoError || !doc.isObject() ||
        !root["version"].isDouble() || root["version"].toDouble() != 1 ||
        !root["receipts"].isArray())
      throw std::runtime_error("Неизвестный или повреждённый формат "
                               "дивидендов; запись заблокирована");
    QList<QVariantMap> candidate;
    QSet<QString> ids, keys;
    for (auto v : root["receipts"].toArray()) {
      if (!v.isObject())
        throw std::runtime_error("Повреждённая выплата");
      auto r = v.toObject().toVariantMap();
      auto id = r["id"].toString();
      auto key = r["key"].toString();
      if (ids.contains(id) || keys.contains(key) || !validReceipt(r))
        throw std::runtime_error(
            "Некорректные поля выплаты; запись заблокирована");
      ids.insert(id);
      keys.insert(key);
      candidate.push_back(r);
    }
    saved_ = candidate;
  } catch (const std::exception &e) {
    valid_ = false;
    error_ = q(e.what());
    saved_.clear();
  }
}
std::map<QString, PortfolioDividendsController::Candidate>
PortfolioDividendsController::calculateCandidates() const {
  std::map<QString, Candidate> result;
  cache_error_.clear();
  QSet<QString> tickers;
  for (const auto &t : market_->journalSnapshot().transactions)
    tickers.insert(q(t.ticker));
  const auto journal = market_->journalSnapshot();
  const auto today = todayMoscow().toStdString();
  for (const auto &ticker : tickers)
    try {
      auto bytes = storage::read_text(
          market_->dataRoot() / ("dividends_" + ticker.toStdString() + ".csv"),
          1024 * 1024);
      if (!bytes)
        continue;
      auto snapshot = parse_dividend_snapshot(*bytes, ticker.toStdString());
      if (!snapshot)
        throw std::runtime_error("Повреждён кэш дивидендов: " +
                                 ticker.toStdString());
      QSet<QString> conflicts;
      for (const auto &event : snapshot->events) {
        auto key = q(dividend_event_key(ticker.toStdString(), event));
        if (conflicts.contains(key))
          continue;
        if (result.contains(key)) {
          result.erase(key);
          conflicts.insert(key);
          continue;
        }
        if (event.status != DividendStatus::Approved ||
            event.verification != DividendVerification::Matched)
          continue;
        auto entitlement = calculate_dividend_entitlement(
            ticker.toStdString(), event, journal.transactions,
            moex_calendar_2026(), today);
        if (entitlement.status != DividendEntitlementStatus::Available ||
            !entitlement.quantity || *entitlement.quantity <= 0 ||
            *entitlement.quantity > 1e12 || event.amount_per_share > 1e12 ||
            !entitlement.gross_amount || *entitlement.gross_amount > 1e15)
          continue;
        QVariantList ps;
        for (const auto &p : entitlement.positions)
          ps.push_back(
              QVariantMap{{"id", q(p.position_id)}, {"quantity", p.quantity}});
        QVariantMap canonical{
            {"key", key},
            {"cache", QString::fromLatin1(QCryptographicHash::hash(
                                              QByteArray::fromStdString(*bytes),
                                              QCryptographicHash::Sha256)
                                              .toHex())},
            {"revision", QString::number(market_->journalRevision())},
            {"quantity", *entitlement.quantity},
            {"gross", *entitlement.gross_amount},
            {"positions", ps}};
        result.emplace(key, Candidate{ticker.toStdString(), event, entitlement,
                                      fingerprint(canonical),
                                      q(snapshot->checked_at)});
      }
    } catch (const std::exception &e) {
      cache_error_ += q(e.what()) + "\n";
    }
  return result;
}
void PortfolioDividendsController::reload() { rebuild(); }
void PortfolioDividendsController::rebuild() {
  candidates_ = calculateCandidates();
  QList<QVariantMap> events, receipts;
  QSet<QString> paid;
  double net = 0, expected = 0, projected = 0;
  QVariantMap positionNet;
  for (auto r : saved_) {
    auto key = r["key"].toString();
    paid.insert(key);
    const auto it = candidates_.find(key);
    bool mismatch = it == candidates_.end();
    bool correction =
        std::abs(r["gross"].toDouble() -
                 r["quantity"].toDouble() * r["perShare"].toDouble()) > 0.005;
    if (!mismatch) {
      QVariantList ps;
      for (const auto &p : it->second.entitlement.positions)
        ps.push_back(
            QVariantMap{{"id", q(p.position_id)}, {"quantity", p.quantity}});
      mismatch = ps != r["positions"].toList() ||
                 std::abs(*it->second.entitlement.quantity -
                          r["quantity"].toDouble()) > 1e-8;
      correction = correction || std::abs(it->second.event.amount_per_share -
                                          r["perShare"].toDouble()) > 1e-8;
    }
    const double received = r["gross"].toDouble() - r["tax"].toDouble();
    net += received;
    r["netText"] = money(received);
    r["journalMismatch"] = mismatch;
    r["correction"] = correction;
    if (auto allocated = allocate_dividend_income(
            positions(r["positions"].toList()), r["gross"].toDouble(),
            r["tax"].toDouble()))
      for (auto &[id, value] : *allocated)
        positionNet[q(id)] = positionNet[q(id)].toDouble() + value;
    receipts.push_back(r);
  }
  for (const auto &[key, c] : candidates_) {
    if (paid.contains(key))
      continue;
    const double gross = *c.entitlement.gross_amount;
    if (c.entitlement.projected)
      projected += gross;
    else
      expected += gross;
    events.push_back(
        {{"key", key},
         {"ticker", q(c.ticker)},
         {"recordDate", q(c.event.record_date)},
         {"period", q(c.event.period)},
         {"grossText", money(gross)},
         {"projected", c.entitlement.projected},
         {"canConfirm",
          !c.entitlement.projected && q(c.event.record_date) <= todayMoscow()},
         {"sourceFresh", QDate::fromString(c.checkedAt.left(10), Qt::ISODate) ==
                             QDate::fromString(todayMoscow(), Qt::ISODate)},
         {"checkedAt", c.checkedAt},
         {"reason", q(c.entitlement.reason)}});
  }
  events_.replace(events);
  receipts_.replace(receipts);
  if (valid_ && !cache_error_.isEmpty())
    error_ = cache_error_;
  const bool totalsValid = valid_ && std::isfinite(net) &&
                           std::isfinite(expected) &&
                           std::isfinite(projected) && net <= 1e18 &&
                           expected <= 1e18 && projected <= 1e18;
  totals_ = {{"valid", totalsValid},
             {"receivedNet", totalsValid ? QVariant(net) : QVariant()},
             {"receivedNetText",
              totalsValid ? money(net) : QStringLiteral("Недоступно")},
             {"expectedGross", totalsValid ? QVariant(expected) : QVariant()},
             {"expectedGrossText",
              totalsValid ? money(expected) : QStringLiteral("Недоступно")},
             {"projectedGross", totalsValid ? QVariant(projected) : QVariant()},
             {"projectedGrossText",
              totalsValid ? money(projected) : QStringLiteral("Недоступно")},
             {"positionNet", totalsValid ? positionNet : QVariantMap{}}};
  emit changed();
}
QVariantMap PortfolioDividendsController::preview(QString key) const {
  auto current = calculateCandidates();
  auto i = current.find(key);
  if (i == current.end() || i->second.entitlement.projected ||
      q(i->second.event.record_date) > todayMoscow())
    return {};
  for (const auto &r : saved_)
    if (r["key"] == key)
      return {};
  const auto &c = i->second;
  return {{"key", key},
          {"previewToken", c.token},
          {"ticker", q(c.ticker)},
          {"gross", QString::number(*c.entitlement.gross_amount, 'f', 2)},
          {"recordDate", q(c.event.record_date)},
          {"quantity", *c.entitlement.quantity},
          {"source", q(c.event.source_url)}};
}
bool PortfolioDividendsController::save(const QList<QVariantMap> &candidate) {
  if (!writable()) {
    error_ = "Запись выплат недоступна";
    emit changed();
    return false;
  }
  try {
    const auto disk = storage::read_text(
        market_->dataRoot() / "portfolio_dividend_receipts_v1.json",
        2 * 1024 * 1024);
    const auto hash =
        disk ? QCryptographicHash::hash(QByteArray::fromStdString(*disk),
                                        QCryptographicHash::Sha256)
             : QByteArray{};
    if (hash != persisted_hash_) {
      valid_ = false;
      throw std::runtime_error(
          "Файл выплат изменён извне; запись заблокирована до перезапуска");
    }
    QJsonArray rows;
    QSet<QString> ids, keys;
    double total = 0;
    for (const auto &r : candidate) {
      if (!validReceipt(r) || ids.contains(r["id"].toString()) ||
          keys.contains(r["key"].toString()))
        throw std::runtime_error("Некорректная выплата");
      ids.insert(r["id"].toString());
      keys.insert(r["key"].toString());
      total += r["gross"].toDouble() - r["tax"].toDouble();
      if (!std::isfinite(total) || total > 1e18)
        throw std::runtime_error("Сумма выплат выходит за диапазон");
      rows.push_back(QJsonObject::fromVariantMap(r));
    }
    auto bytes =
        QJsonDocument(QJsonObject{{"version", 1}, {"receipts", rows}}).toJson();
    if (bytes.size() > 2 * 1024 * 1024)
      throw std::runtime_error("Слишком много выплат");
    storage::atomic_write(market_->dataRoot() /
                              "portfolio_dividend_receipts_v1.json",
                          bytes.toStdString());
    persisted_hash_ =
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    saved_ = candidate;
    error_.clear();
    rebuild();
    return true;
  } catch (const std::exception &e) {
    error_ = q(e.what());
    if(!valid_)rebuild();else emit changed();
    return false;
  }
}
bool PortfolioDividendsController::confirm(QString key, QString paymentDate,
                                           QString grossText, QString taxText,
                                           QString reference, QString token) {
  auto current = calculateCandidates();
  auto i = current.find(key);
  bool gok = false, tok = false;
  double gross = grossText.replace(',', '.').toDouble(&gok),
         tax = taxText.replace(',', '.').toDouble(&tok);
  reference = reference.trimmed();
  if (i == current.end() || token.isEmpty() || token != i->second.token ||
      i->second.entitlement.projected ||
      q(i->second.event.record_date) > todayMoscow()) {
    error_ = "Данные изменились. Откройте подтверждение заново";
    emit changed();
    return false;
  }
  const auto &c = i->second;
  for (const auto &r : saved_)
    if (r["key"] == key) {
      error_ = "Выплата уже подтверждена";
      emit changed();
      return false;
    }
  if (!date(paymentDate) || paymentDate < q(c.event.record_date) ||
      paymentDate > todayMoscow() || !gok || !tok || !std::isfinite(gross) ||
      !std::isfinite(tax) || gross <= 0 || gross > 1e15 || tax < 0 ||
      tax > gross || reference.isEmpty() || reference.size() > 1000) {
    error_ =
        "Проверьте дату поступления, сумму, удержанный налог и подтверждение";
    emit changed();
    return false;
  }
  QVariantList ps;
  for (const auto &p : c.entitlement.positions)
    ps.push_back(
        QVariantMap{{"id", q(p.position_id)}, {"quantity", p.quantity}});
  auto next = saved_;
  next.push_back(
      {{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
       {"key", key},
       {"ticker", q(c.ticker)},
       {"recordDate", q(c.event.record_date)},
       {"cutoff",
        q(*dividend_last_buy(c.event.record_date, moex_calendar_2026()))},
       {"period", q(c.event.period)},
       {"status", 3.0},
       {"verification", 1.0},
       {"quantity", *c.entitlement.quantity},
       {"perShare", c.event.amount_per_share},
       {"gross", gross},
       {"tax", tax},
       {"paymentDate", paymentDate},
       {"reference", reference},
       {"source", q(c.event.source_url)},
       {"recordedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
       {"checkedAt", c.checkedAt},
       {"journalRevision", QString::number(market_->journalRevision())},
       {"positions", ps}});
  return save(next);
}
bool PortfolioDividendsController::removeReceipt(QString id) {
  auto next = saved_;
  auto old = next.size();
  next.removeIf([&](const auto &r) { return r["id"] == id; });
  return next.size() != old && save(next);
}
std::vector<BacktestCashDividend>
PortfolioDividendsController::backtestDividends(
    const std::string &ticker) const {
  std::vector<BacktestCashDividend> result;
  if (!valid_)
    return result;
  auto current = calculateCandidates();
  for (const auto &r : saved_) {
    auto i = current.find(r["key"].toString());
    if (r["ticker"].toString() != q(ticker) || i == current.end() ||
        std::abs(r["quantity"].toDouble() * i->second.event.amount_per_share -
                 r["gross"].toDouble()) > 0.005)
      continue;
    QVariantList ps;
    for (const auto &p : i->second.entitlement.positions)
      ps.push_back(
          QVariantMap{{"id", q(p.position_id)}, {"quantity", p.quantity}});
    if (ps != r["positions"].toList() ||
        std::abs(*i->second.entitlement.quantity - r["quantity"].toDouble()) >
            1e-8 ||
        std::abs(i->second.event.amount_per_share - r["perShare"].toDouble()) >
            1e-8 ||
        std::abs(r["quantity"].toDouble() * r["perShare"].toDouble() -
                 r["gross"].toDouble()) > 0.005)
      continue;
    result.push_back({r["cutoff"].toString().toStdString(),
                      r["paymentDate"].toString().toStdString(),
                      r["perShare"].toDouble(),
                      r["id"].toString().toStdString()});
  }
  return result;
}
