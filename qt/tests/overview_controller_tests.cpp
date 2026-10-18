#include "portfolio_controller.hpp"
#include "storage.hpp"
#include <QDate>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <future>
using namespace moexwatch;
namespace {
std::filesystem::path rootOf(const QTemporaryDir& dir) {
#ifdef _WIN32
    return dir.path().toStdWString();
#else
    return dir.path().toStdString();
#endif
}
Instrument instrument(const char* ticker) { Instrument item;item.ticker=ticker;item.quote.ticker=ticker;return item; }
std::vector<Candle> history(int count,double base=100) {
    std::vector<Candle> result;
    for(int n=0;n<count;++n){const auto price=base+n;result.push_back({QDate(2025,1,1).addDays(n).toString(Qt::ISODate).toStdString(),price,price+2,price-2,price,1000});}
    return result;
}
void seed(const std::filesystem::path& root) { AppStorage(root).save_watchlist({instrument("SBER"),instrument("GAZP")}); }
}
class OverviewTests:public QObject {
    Q_OBJECT
private slots:
    void recoveredRefreshClearsOnlyItsNetworkError() {
        QTemporaryDir dir;const auto root=rootOf(dir);seed(root);bool fails=true;
        PortfolioController c(root,[&](auto items,std::stop_token)->MarketRefreshResult {
            if(fails)throw std::runtime_error("injected outage");
            for(auto& i:items){i.quote.last=150;i.quote_stale=false;i.candles=history(25);}
            return {false,std::move(items),history(25,2500),{}};
        });
        c.refresh();QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),3000);QVERIFY(!c.error().isEmpty());
        fails=false;c.refresh();QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),3000);QVERIFY(c.error().isEmpty());
        fails=true;c.refresh();QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),3000);QVERIFY(!c.error().isEmpty());
        QVERIFY(!c.setTarget("SBER","-1"));const auto validation=c.error();
        fails=false;c.refresh();QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),3000);QCOMPARE(c.error(),validation);
    }
    void chartLevelsFollowCurrentPositionIdentity() {
        QTemporaryDir dir;const auto root=rootOf(dir);seed(root);PortfolioController c(root);
        c.setSelectedTicker("SBER");QVERIFY(c.addOperation("2026-10-01","SBER",0,"2","100","0"));
        const auto first=c.positions()->rows().first()["id"].toString();QVERIFY(c.saveLevels(first,"90","130"));
        QCOMPARE(c.chartLevels()["average"].toDouble(),100.0);QCOMPARE(c.chartLevels()["stop"].toDouble(),90.0);
        QVERIFY(c.addOperation("2026-10-01","SBER",1,"2","110","0"));QCOMPARE(c.chartLevels()["average"].toDouble(),0.0);
        QVERIFY(c.addOperation("2026-10-01","SBER",0,"1","120","0"));
        QVERIFY(c.positions()->rows().first()["id"].toString()!=first);
        QCOMPARE(c.chartLevels()["average"].toDouble(),120.0);QCOMPARE(c.chartLevels()["stop"].toDouble(),0.0);QCOMPARE(c.chartLevels()["take"].toDouble(),0.0);
    }
    void refreshPreservesUserTargetAndRemovedTicker() {
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto root=rootOf(dir);seed(root);
        std::promise<void> ready,release;auto released=release.get_future().share();
        PortfolioController c(root,[&](auto items,std::stop_token)->MarketRefreshResult {
            ready.set_value();released.wait();for(auto& i:items){i.quote.last=150;i.candles=history(220);}
            return {false,std::move(items),history(220,2500),{}};
        });
        QVERIFY(c.setTarget("SBER","110"));c.refresh();ready.get_future().wait();
        QVERIFY(c.setTarget("SBER","125,5"));QVERIFY(c.removeTicker("GAZP"));c.setSelectedTicker("SBER");
        release.set_value();QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),3000);
        QCOMPARE(c.analysis()["target"].toDouble(),125.5);QCOMPARE(c.selectedTicker(),QString("SBER"));
        QCOMPARE(c.watchlist()->rowCount(),1);QVERIFY(!c.tickers().contains("GAZP"));
        QCOMPARE(AppStorage(root).load_watchlist().front().target.value(),125.5);
    }
    void failedAndInvalidIndexKeepCachedValues() {
        QTemporaryDir dir;const auto root=rootOf(dir);seed(root);
        storage::save_candles(root,{"SBER","cached",history(25)});
        storage::save_candles(root,{"IMOEX","cached",history(25,2500)});
        int call=0;
        PortfolioController c(root,[&](auto items,std::stop_token)->MarketRefreshResult {
            ++call;for(auto& i:items)i.candles.clear();auto index=history(25,9000);if(call==1)index[0].high=1;
            return {false,std::move(items),std::move(index),call==2?"index unavailable":""};
        });
        c.setSelectedTicker("SBER");QCOMPARE(c.chartCandles().first().toMap()["benchmark"].toDouble(),2500.0);
        for(int n=0;n<2;++n){c.refresh();QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),3000);
            QCOMPARE(c.chartCandles().first().toMap()["benchmark"].toDouble(),2500.0);
            QCOMPARE(c.chartCandles().size(),25);QCOMPARE(c.chartCandles().last().toMap()["close"].toDouble(),124.0);
            QVERIFY(c.analysis()["historyStatus"].toString().contains("не подтверждена"));
            QVERIFY(c.analysis()["indexStatus"].toString().contains("неполные"));QVERIFY(!c.analysis()["indexError"].toString().isEmpty());}
    }
    void fullHistoryWarmupAndDateMatchedBenchmark() {
        QTemporaryDir dir;const auto root=rootOf(dir);seed(root);
        storage::save_candles(root,{"SBER","cached",history(220)});
        auto benchmark=history(220,2500);benchmark.erase(benchmark.begin()+200);
        storage::save_candles(root,{"IMOEX","cached",benchmark});
        PortfolioController c(root);c.setSelectedTicker("SBER");c.setChartSessions(22);
        const auto bars=c.chartCandles();QCOMPARE(bars.size(),220);
        QVERIFY(!bars[13].toMap()["rsi"].isValid());QVERIFY(bars[14].toMap()["rsi"].isValid());
        QVERIFY(!bars[198].toMap()["sma200"].isValid());QCOMPARE(bars[199].toMap()["sma200"].toDouble(),199.5);
        QCOMPARE(bars.last().toMap()["sma200"].toDouble(),219.5);QCOMPARE(c.analysis()["sma200"].toDouble(),219.5);
        QVERIFY(!bars[200].toMap().contains("benchmark"));QCOMPARE(bars[201].toMap()["benchmark"].toDouble(),2701.0);
        c.setSelectedTicker("GAZP");QVERIFY(c.chartCandles().isEmpty());QVERIFY(!c.analysis()["rsi"].isValid());
    }
    void selectionSettingsAndValidation() {
        QTemporaryDir dir;const auto root=rootOf(dir);seed(root);
        {
            PortfolioController c(root);c.setSelectedTicker("GAZP");c.setChartSessions(252);c.setChartComparison(true);
            c.setSelectedTicker("INVALID");QCOMPARE(c.selectedTicker(),QString("GAZP"));
            c.setChartSessions(-1);QCOMPARE(c.chartSessions(),252);
            QVERIFY(!c.addTicker("../BAD"));QVERIFY(!c.setTarget("SBER","nan"));QVERIFY(!c.setTarget("SBER","-1"));
            QVERIFY(c.addTicker(" sber "));QCOMPARE(c.watchlist()->rowCount(),2);
            QVERIFY(c.setTarget("SBER","100"));QVERIFY(c.setTarget("SBER",""));QVERIFY(!c.analysis()["target"].isValid());
            c.setSelectedTicker("GAZP");
        }
        PortfolioController c(root);QCOMPARE(c.selectedTicker(),QString("GAZP"));QCOMPARE(c.chartSessions(),252);QVERIFY(c.chartComparison());
        QVERIFY(c.removeTicker("GAZP"));QCOMPARE(c.selectedTicker(),QString("SBER"));
        QVERIFY(c.addOperation("2026-10-01","SBER",0,"1","100","0"));QVERIFY(!c.removeTicker("SBER"));
    }
};
QTEST_GUILESS_MAIN(OverviewTests)
#include "overview_controller_tests.moc"
