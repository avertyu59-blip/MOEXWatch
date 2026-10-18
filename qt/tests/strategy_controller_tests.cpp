#include "backtest.hpp"
#include "portfolio_controller.hpp"
#include "strategy_controller.hpp"
#include <QDate>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
using namespace moexwatch;
namespace {
std::vector<Candle> history(int n) {
    std::vector<Candle> result;
    for (int i = 0; i < n; ++i) {
        double price = 100 + i;
        result.push_back({QDate(2025, 1, 1).addDays(i).toString(Qt::ISODate).toStdString(), price, price + 1,
                          price - 1, price, 100});
    }
    return result;
}
QVariantMap rules() {
    return {{"oversoldEnabled", false},
            {"overboughtEnabled", false},
            {"aboveSma200", true},
            {"nearTarget", false},
            {"requireAll", false}};
}
} // namespace
class StrategyTests : public QObject {
    Q_OBJECT
  private slots:
    void validResultMatchesCore() {
        QTemporaryDir dir;
        auto candles = history(240);
        PortfolioController market(dir.path().toStdString(), [candles](auto items, std::stop_token) {
            for (auto &i : items) {
                i.candles = candles;
                i.candles_stale = false;
            }
            return MarketRefreshResult{false, items, candles, {}};
        });
        market.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!market.busy(), 3000);
        StrategyController c(&market);
        QVERIFY(c.run("100000", "0,05", "", "", rules()));
        QVERIFY(c.busy());
        QVERIFY(!c.run("100000", "0.05", "", "", rules()));
        QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 3000);
        QVERIFY2(c.error().isEmpty(), qPrintable(c.error()));
        auto r = c.result();
        SignalSettings settings;
        settings.rsi_oversold_enabled = false;
        settings.near_target_enabled = false;
        auto expected = run_backtest(candles, candles, settings, {}, 100000, .05);
        QVERIFY(expected);
        QCOMPARE(r["strategyFinal"].toDouble(), expected->strategy_final_value);
        QCOMPARE(r["trades"].toInt(), 1);
        QCOMPARE(r["commonSessions"].toInt(), 240);
        QVERIFY(r["historyStale"].toBool());
        QVERIFY(market.addTicker("TEST"));
        market.setSelectedTicker("TEST");
        QVERIFY(c.result()["outdated"].toBool());
    }
    void validatesParameters() {
        QTemporaryDir dir;
        PortfolioController market(dir.path().toStdString());
        StrategyController c(&market);
        QVERIFY(!c.run("nan", "0", "", "", rules()));
        QVERIFY(!c.run("0", "0", "", "", rules()));
        QVERIFY(!c.run("100", "10.01", "", "", rules()));
        QVERIFY(!c.run("100", "0", "2025-02-30", "", rules()));
        QVERIFY(!c.run("100", "0", "2025-02-02", "2025-01-01", rules()));
        auto disabled = rules();
        disabled["aboveSma200"] = false;
        QVERIFY(!c.run("100", "0", "", "", disabled));
        auto invalid = rules();
        invalid["oversoldBelow"] = "nan";
        QVERIFY(!c.run("100", "0", "", "", invalid));
        QVERIFY(!c.busy());
        QVERIFY(c.result().isEmpty());
        c.clearError();
        QVERIFY(c.error().isEmpty());
    }
    void missingJointDatesFails() {
        QTemporaryDir dir;
        auto candles = history(20);
        auto other = candles;
        for (auto &c : other)
            c.date = QDate::fromString(QString::fromStdString(c.date), Qt::ISODate)
                         .addYears(-1)
                         .toString(Qt::ISODate)
                         .toStdString();
        PortfolioController market(dir.path().toStdString(), [candles, other](auto items, std::stop_token) {
            for (auto &i : items) {
                i.candles = candles;
                i.candles_stale = false;
            }
            return MarketRefreshResult{false, items, other, {}};
        });
        market.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!market.busy(), 3000);
        StrategyController c(&market);
        QVERIFY(!c.run("10000", "0.05", "", "", rules()));
        QVERIFY(c.error().contains(QStringLiteral("общих")));
    }
    void selectedPeriodIsApplied() {
        QTemporaryDir dir;
        auto candles = history(240);
        PortfolioController market(dir.path().toStdString(), [candles](auto items, std::stop_token) {
            for (auto &i : items) {
                i.candles = candles;
                i.candles_stale = false;
            }
            return MarketRefreshResult{false, items, candles, {}};
        });
        market.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!market.busy(), 3000);
        StrategyController c(&market);
        QVERIFY(c.run("10000", "0", "2025-01-11", "2025-01-30", rules()));
        QTRY_VERIFY_WITH_TIMEOUT(!c.busy(), 3000);
        QCOMPARE(c.result()["from"].toString(), QString("2025-01-11"));
        QCOMPARE(c.result()["to"].toString(), QString("2025-01-30"));
        QCOMPARE(c.result()["commonSessions"].toInt(), 20);
        QVERIFY(c.result()["smaWarmup"].toBool());
        QCOMPARE(c.result()["trades"].toInt(), 0);
    }
};
QTEST_GUILESS_MAIN(StrategyTests)
#include "strategy_controller_tests.moc"
