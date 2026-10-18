#include "market_core.hpp"
#include "portfolio_dividends_controller.hpp"
#include "storage.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
using namespace moexwatch;
namespace {
void cache(const std::filesystem::path &root, double amount = 10,
           DividendStatus status = DividendStatus::Approved) {
  DividendEvent e;
  e.record_date = "2026-10-16";
  e.amount_per_share = amount;
  e.currency = "RUB";
  e.status = status;
  e.verification = DividendVerification::Matched;
  e.period = "2025";
  e.source_url = "https://example.org/dividend";
  e.last_buy_date = *dividend_last_buy(e.record_date, moex_calendar_2026());
  storage::atomic_write(
      root / "dividends_SBER.csv",
      serialize_dividend_snapshot({"SBER", "2026-10-16", {e}}));
}
QString key() { return "SBER|2026-10-16|2025"; }
} // namespace
class DividendTests : public QObject {
  Q_OBJECT
private slots:
  void themeAndUnchangedReloadKeepIncomeStable() {
    QTemporaryDir dir;PortfolioController market(dir.path().toStdString());
    PortfolioDividendsController c(&market);QSignalSpy spy(&c,&PortfolioDividendsController::incomeChanged);
    c.reload();market.setDark(false);QVERIFY(market.setFreeCash("123"));
    QCOMPARE(spy.count(),0);
  }
  void receiveRestartCloseAndAudit() {
    QTemporaryDir dir;
    auto root = std::filesystem::path(dir.path().toStdString());
    {
      PortfolioController market(root);
      QVERIFY(market.addOperation("2026-10-01", "SBER", 0, "10", "100", "0"));
      cache(root);
      PortfolioDividendsController c(&market);
      QCOMPARE(c.events()->rowCount(), 1);
      QCOMPARE(c.totals()["receivedNet"].toDouble(), 0.);
      auto p = c.preview(key());
      QVERIFY(!p["previewToken"].toString().isEmpty());
      QVERIFY(!c.confirm(key(), "2026-10-17", "100", "13", "report", ""));
      QVERIFY(c.confirm(key(), "2026-10-17", "100", "13", "report",
                        p["previewToken"].toString()));
      QCOMPARE(c.totals()["receivedNet"].toDouble(), 87.);
      QVERIFY(!c.confirm(key(), "2026-10-17", "100", "13", "report",
                         p["previewToken"].toString()));
      QVERIFY(market.addOperation("2026-10-18", "SBER", 1, "10", "110", "0"));
      QCOMPARE(c.receipts()->rowCount(), 1);
      QCOMPARE(c.backtestDividends("SBER").size(), std::size_t(1));
    }
    {
      PortfolioController market(root);
      PortfolioDividendsController c(&market);
      QCOMPARE(c.totals()["receivedNet"].toDouble(), 87.);
      QVERIFY(c.writable());
      QFile::remove(
          QString::fromStdString((root / "dividends_SBER.csv").string()));
      c.reload();
      QCOMPARE(c.totals()["receivedNet"].toDouble(), 87.);
      QVERIFY(c.receipts()->rows().first()["journalMismatch"].toBool());
      QVERIFY(c.backtestDividends("SBER").empty());
    }
  }
  void stalePreview() {
    QTemporaryDir dir;
    auto root = std::filesystem::path(dir.path().toStdString());
    PortfolioController market(root);
    QVERIFY(market.addOperation("2026-10-01", "SBER", 0, "10", "100", "0"));
    cache(root);
    PortfolioDividendsController c(&market);
    auto p = c.preview(key());
    cache(root, 11);
    QVERIFY(!c.confirm(key(), "2026-10-17", "100", "0", "report",
                       p["previewToken"].toString()));
    p = c.preview(key());
    QVERIFY(market.addOperation("2026-10-02", "SBER", 0, "1", "100", "0"));
    QVERIFY(!c.confirm(key(), "2026-10-17", "110", "0", "report",
                       p["previewToken"].toString()));
  }
  void corruptAndFractionalVersion() {
    for (const auto &bytes :
         {QByteArray("corrupt"),
          QByteArray("{\"version\":1.5,\"receipts\":[]}")}) {
      QTemporaryDir dir;
      auto root = std::filesystem::path(dir.path().toStdString());
      auto file = root / "portfolio_dividend_receipts_v1.json";
      storage::atomic_write(file, bytes.toStdString());
      PortfolioController market(root);
      PortfolioDividendsController c(&market);
      QVERIFY(!c.writable());
      QVERIFY(!c.totals()["valid"].toBool());
      QCOMPARE(*storage::read_text(file, 1024), bytes.toStdString());
    }
  }
  void invalidReceiptEnum() {
    QTemporaryDir dir;
    auto root = std::filesystem::path(dir.path().toStdString());
    {
      PortfolioController market(root);
      QVERIFY(market.addOperation("2026-10-01", "SBER", 0, "10", "100", "0"));
      cache(root);
      PortfolioDividendsController c(&market);
      auto p = c.preview(key());
      QVERIFY(c.confirm(key(), "2026-10-17", "100", "0", "report",
                        p["previewToken"].toString()));
    }
    auto path = root / "portfolio_dividend_receipts_v1.json";
    auto obj = QJsonDocument::fromJson(
                   QByteArray::fromStdString(*storage::read_text(path, 100000)))
                   .object();
    auto rows = obj["receipts"].toArray();
    auto row = rows[0].toObject();
    row["status"] = 99;
    rows[0] = row;
    obj["receipts"] = rows;
    storage::atomic_write(path, QJsonDocument(obj).toJson().toStdString());
    PortfolioController market(root);
    PortfolioDividendsController c(&market);
    QVERIFY(!c.writable());
  }
  void sameDayCyclesAndDeletedJournal() {
    QTemporaryDir dir;
    auto root = std::filesystem::path(dir.path().toStdString());
    PortfolioController market(root);
    QVERIFY(market.addOperation("2026-10-01", "SBER", 0, "10", "100", "0"));
    QVERIFY(market.addOperation("2026-10-01", "SBER", 1, "10", "110", "0"));
    QVERIFY(market.addOperation("2026-10-01", "SBER", 0, "5", "105", "0"));
    cache(root);
    PortfolioDividendsController c(&market);
    auto p = c.preview(key());
    QCOMPARE(p["quantity"].toDouble(), 5.);
    QVERIFY(c.confirm(key(), "2026-10-17", "50", "0", "report",
                      p["previewToken"].toString()));
    auto original = c.receipts()->rows().first();
    auto opening = market.operations()->rows().first()["id"].toString();
    // Deleting the whole journal in reverse order preserves the confirmed cash
    // fact.
    while (market.operations()->rowCount() > 0) {
      auto rows = market.operations()->rows();
      bool removed = false;
      for (const auto &row : rows)
        if (market.deleteOperation(row["id"].toString())) {
          removed = true;
          break;
        }
      QVERIFY(removed);
    }
    QCOMPARE(c.totals()["receivedNet"].toDouble(), 50.);
    QVERIFY(c.receipts()->rows().first()["journalMismatch"].toBool());
    QCOMPARE(c.receipts()->rows().first()["positions"], original["positions"]);
  }
  void duplicateKeysAndOverflowBlocked() {
    QTemporaryDir dir;
    auto root = std::filesystem::path(dir.path().toStdString());
    {
      PortfolioController market(root);
      QVERIFY(market.addOperation("2026-10-01", "SBER", 0, "10", "100", "0"));
      cache(root);
      PortfolioDividendsController c(&market);
      auto p = c.preview(key());
      QVERIFY(!c.confirm(key(), "2026-10-17", "1e309", "0", "report",
                         p["previewToken"].toString()));
      QVERIFY(c.confirm(key(), "2026-10-17", "100", "0", "report",
                        p["previewToken"].toString()));
    }
    auto path = root / "portfolio_dividend_receipts_v1.json";
    auto obj = QJsonDocument::fromJson(
                   QByteArray::fromStdString(*storage::read_text(path, 100000)))
                   .object();
    auto rows = obj["receipts"].toArray();
    auto row = rows[0].toObject();
    row["id"] = "3489de88-c84e-47ff-b6af-5bd6a8867867";
    row["perShare"] = 11.;
    rows.append(row);
    obj["receipts"] = rows;
    auto bytes = QJsonDocument(obj).toJson();
    storage::atomic_write(path, bytes.toStdString());
    PortfolioController market(root);
    PortfolioDividendsController c(&market);
    QVERIFY(!c.writable());
    QVERIFY(!c.totals()["valid"].toBool());
    QCOMPARE(*storage::read_text(path, 100000), bytes.toStdString());
  }
  void projectionsAreNeverActual() {
    QTemporaryDir dir;
    auto root = std::filesystem::path(dir.path().toStdString());
    PortfolioController market(root);
    QVERIFY(market.addOperation("2026-10-01", "SBER", 0, "10", "100", "0"));
    cache(root, 10, DividendStatus::Forecast);
    PortfolioDividendsController c(&market);
    QVERIFY(c.preview(key()).isEmpty());
    QCOMPARE(c.events()->rowCount(), 0);
    QCOMPARE(c.totals()["receivedNet"].toDouble(), 0.);
  }
};
QTEST_GUILESS_MAIN(DividendTests)
#include "portfolio_dividends_tests.moc"
