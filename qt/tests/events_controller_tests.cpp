#include "events_controller.hpp"
#include "storage.hpp"
#include <QTemporaryDir>
#include <QTest>
#include <atomic>
using namespace moexwatch;
namespace {
std::filesystem::path rootOf(const QTemporaryDir& d){
#ifdef _WIN32
return d.path().toStdWString();
#else
return d.path().toStdString();
#endif
}
void seed(const std::filesystem::path& root){Instrument a;a.ticker="SBER";Instrument b;b.ticker="GAZP";AppStorage(root).save_watchlist({a,b});}
DividendFetchResult response(std::string ticker){DividendFetchResult r;r.ticker=std::move(ticker);r.checked_at="2026-10-18T10:00:00Z";DividendEvent e;e.record_date="2026-10-30";e.amount_per_share=20;e.currency="RUB";e.status=DividendStatus::Recommended;e.source_url="https://smart-lab.ru/dividends/";r.events.push_back(e);return r;}
}
class EventTests:public QObject {
 Q_OBJECT
private slots:
 void corporateRestartAndCorruption(){QTemporaryDir dir;const auto root=rootOf(dir);seed(root);QString id;
 {PortfolioController m(root);m.setSelectedTicker("SBER");EventsController c(&m);QVERIFY(c.addCorporate("2026-11-01","Отчётность","МСФО","https://example.com/report","Подтверждено"));id=c.corporate()->rows()[0]["id"].toString();QVERIFY(!id.isEmpty());QVERIFY(!c.addCorporate("bad","Отчётность","a","https://example.com","Подтверждено"));}
 {PortfolioController m(root);m.setSelectedTicker("SBER");EventsController c(&m);QCOMPARE(c.corporate()->rows()[0]["id"].toString(),id);QVERIFY(c.removeCorporate(id));QCOMPARE(c.corporate()->rowCount(),0);}
 storage::atomic_write(root/"corporate_events_v1.json","broken");
 {PortfolioController m(root);m.setSelectedTicker("SBER");EventsController c(&m);QVERIFY(!c.calendarWritable());QVERIFY(!c.addCorporate("2026-11-01","Собрание","a","https://example.com","Запланировано"));QCOMPARE(*storage::read_text(root/"corporate_events_v1.json",1024),std::string("broken"));}
 }
 void refreshCacheAndRestart(){QTemporaryDir dir;const auto root=rootOf(dir);seed(root);
 {PortfolioController market(root);market.setSelectedTicker("SBER");EventsController c(&market,[](auto ticker,auto){return response(ticker);});c.refresh();QTRY_VERIFY(!c.busy());QCOMPARE(c.dividends()->rowCount(),1);QVERIFY(!c.dividends()->rows()[0]["stale"].toBool());QVERIFY(!c.dividends()->rows()[0].contains("yield"));}
 {PortfolioController market(root);market.setSelectedTicker("SBER");EventsController c(&market);QCOMPARE(c.dividends()->rowCount(),1);QVERIFY(c.dividends()->rows()[0]["stale"].toBool());}
 }
 void failedRefreshPreservesSnapshot(){QTemporaryDir dir;const auto root=rootOf(dir);seed(root);bool fail=false;PortfolioController market(root);market.setSelectedTicker("SBER");EventsController c(&market,[&](auto ticker,auto){auto r=response(ticker);if(fail)r.error="outage";return r;});c.refresh();QTRY_VERIFY(!c.busy());fail=true;c.refresh();QTRY_VERIFY(!c.busy());QCOMPARE(c.dividends()->rowCount(),1);QVERIFY(!c.error().isEmpty());QVERIFY(c.dividends()->rows()[0]["stale"].toBool());}
 void rejectsFalseApproval(){QTemporaryDir dir;const auto root=rootOf(dir);seed(root);PortfolioController market(root);market.setSelectedTicker("SBER");EventsController c(&market,[](auto ticker,auto){auto r=response(ticker);r.events[0].status=DividendStatus::Approved;return r;});c.refresh();QTRY_VERIFY(!c.busy());QCOMPARE(c.dividends()->rowCount(),0);QVERIFY(!c.error().isEmpty());}
 void corruptCache(){QTemporaryDir dir;const auto root=rootOf(dir);seed(root);storage::atomic_write(root/"dividends_SBER.csv","broken");PortfolioController market(root);market.setSelectedTicker("SBER");EventsController c(&market);QCOMPARE(c.dividends()->rowCount(),0);QVERIFY(!c.error().isEmpty());}
 void switchCancelsLateResponse(){QTemporaryDir dir;const auto root=rootOf(dir);seed(root);PortfolioController market(root);market.setSelectedTicker("SBER");std::atomic<bool> started=false;EventsController c(&market,[&](auto ticker,std::stop_token stop){started=true;while(!stop.stop_requested())std::this_thread::yield();return response(ticker);});c.refresh();QTRY_VERIFY(started.load());market.setSelectedTicker("GAZP");QCOMPARE(c.ticker(),QString("GAZP"));QTest::qWait(100);QCOMPARE(c.dividends()->rowCount(),0);QVERIFY(!c.busy());}
 void shutdownRequestsCancellation(){QTemporaryDir dir;const auto root=rootOf(dir);seed(root);PortfolioController market(root);market.setSelectedTicker("SBER");std::atomic<bool> started=false,stopped=false;EventsController c(&market,[&](auto ticker,std::stop_token stop){started=true;while(!stop.stop_requested())std::this_thread::yield();stopped=true;return response(ticker);});c.refresh();QTRY_VERIFY(started.load());c.shutdown();QVERIFY(stopped.load());QVERIFY(!c.busy());}
};
QTEST_GUILESS_MAIN(EventTests)
#include "events_controller_tests.moc"
