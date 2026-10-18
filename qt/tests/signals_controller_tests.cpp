#include "signals_controller.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <limits>
using namespace moexwatch;
namespace {
QVariantMap rules() {
    return {{"oversold", true},        {"oversoldBelow", 30.0}, {"overbought", false},
            {"overboughtAbove", 70.0}, {"aboveSma", false},     {"nearTarget", false},
            {"targetPercent", 5.0},    {"all", false},          {"notifications", true},
            {"autoRefresh", false},    {"minutes", 15}};
}
void seed(const std::filesystem::path &root) {
    Instrument a;
    a.ticker = "SBER";
    a.quote.ticker = a.ticker;
    Instrument b;
    b.ticker = "GAZP";
    b.quote.ticker = b.ticker;
    AppStorage(root).save_watchlist({a, b});
}
} // namespace
class SignalsTests final : public QObject {
    Q_OBJECT
  private slots:
    void settingsRoundTripAndRejectedSave() {
        QTemporaryDir d;
        const auto p = std::filesystem::path(d.path().toStdString());
        {
            PortfolioController m(p);
            SignalsController s(&m);
            auto map = rules();
            QVERIFY(s.saveSettings(map));
            map["minutes"] = 1;
            QVERIFY(!s.saveSettings(map));
            QCOMPARE(s.settings()["minutes"].toInt(), 15);
            map["oversoldBelow"] = std::numeric_limits<double>::quiet_NaN();
            map["minutes"] = 15;
            QVERIFY(!s.saveSettings(map));
        }
        PortfolioController m(p);
        SignalsController s(&m);
        QCOMPARE(s.settings()["autoRefresh"].toBool(), false);
        QCOMPARE(s.settings()["oversoldBelow"].toDouble(), 30.0);
    }
    void transitionDedupStaleAndHistoryRestart() {
        QTemporaryDir d;
        const auto p = std::filesystem::path(d.path().toStdString());
        seed(p);
        double quote = 120;
        bool stale = false;
        {
            PortfolioController m(p, [&](auto items, std::stop_token) {
                auto stamp = observation_time(QDateTime::currentSecsSinceEpoch() - (stale ? 7200 : 0));
                for (auto &i : items) {
                    i.quote = {i.ticker, quote, 0, stamp.substr(11), stamp};
                    i.quote_stale = false;
                }
                return MarketRefreshResult{false, std::move(items), {}, {}};
            });
            SignalsController s(&m);
            QVERIFY(s.saveSettings(rules()));
            QSignalSpy notice(&s, &SignalsController::notification);
            m.refresh();
            QTRY_VERIFY_WITH_TIMEOUT(!m.busy(), 3000);
            QVERIFY(s.addAlert("SBER", true, "110"));
            QCOMPARE(notice.count(), 0);
            quote = 100;
            m.refresh();
            QTRY_VERIFY_WITH_TIMEOUT(!m.busy(), 3000);
            QCOMPARE(notice.count(), 1);
            QCOMPARE(s.history()->rowCount(), 1);
            s.rebuild();
            s.rebuild();
            QCOMPARE(notice.count(), 1);
            quote = 120;
            m.refresh();
            QTRY_VERIFY_WITH_TIMEOUT(!m.busy(), 3000);
            stale = true;
            quote = 90;
            m.refresh();
            QTRY_VERIFY_WITH_TIMEOUT(!m.busy(), 3000);
            QCOMPARE(notice.count(), 1);
        }
        PortfolioController restored(p);
        SignalsController s(&restored);
        QCOMPARE(s.history()->rowCount(), 1);
        QCOMPARE(s.alerts()->rowCount(), 1);
    }
    void numericSortMissingLastAndFilters() {
        QTemporaryDir d;
        const auto p = std::filesystem::path(d.path().toStdString());
        seed(p);
        PortfolioController m(p, [](auto items, std::stop_token) {
            const auto stamp = observation_time(QDateTime::currentSecsSinceEpoch());
            for (auto &i : items)
                if (i.ticker == "SBER") {
                    i.quote = {i.ticker, 100, 0, stamp.substr(11), stamp};
                    i.quote_stale = false;
                }
            return MarketRefreshResult{false, std::move(items), {}, {}};
        });
        SignalsController s(&m);
        m.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!m.busy(), 3000);
        s.sort("price", false);
        QCOMPARE(s.screener()->rows().first()["ticker"].toString(), QString("SBER"));
        QVERIFY(!s.screener()->rows().last()["price"].isValid());
        s.sort("price", true);
        QCOMPARE(s.screener()->rows().first()["ticker"].toString(), QString("SBER"));
        QVERIFY(s.setFilters({{"kind", 1}, {"volumeMin", 0}, {"atrMin", 0}}));
        QCOMPARE(s.screener()->rowCount(), 0);
        QVERIFY(!s.setFilters({{"kind", 0}, {"volumeMin", -1}, {"atrMin", 0}}));
        QCOMPARE(s.filters()["kind"].toInt(), 1);
    }
    void corruptAlertsPreservedAndDemoNoWrites() {
        QTemporaryDir d;
        const auto p = std::filesystem::path(d.path().toStdString());
        QFile f(d.path() + "/price_alerts.csv");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("broken");
        f.close();
        PortfolioController m(p);
        SignalsController s(&m);
        QVERIFY(!s.writable());
        QVERIFY(!s.addAlert("SBER", true, "100"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("broken"));
        f.close();
        QTemporaryDir demo;
        PortfolioController dm(demo.path().toStdString());
        dm.loadDemo();
        SignalsController ds(&dm);
        QVERIFY(!ds.writable());
        QVERIFY(!ds.saveSettings(rules()));
        QVERIFY(!QFile::exists(demo.path() + "/price_alerts.csv"));
    }
    void diskFailureKeepsSettingsAndAlertModel() {
        QTemporaryDir d;
        PortfolioController m(d.path().toStdString());
        SignalsController s(&m);
        auto original = s.settings();
        QDir(d.path()).mkdir("signals.ini");
        // An alerts destination occupied by a directory must not mutate configured rules.
        QDir(d.path()).mkdir("price_alerts.csv");
        QVERIFY(!s.addAlert("SBER", true, "100"));
        QCOMPARE(s.alerts()->rowCount(), 0);
        QVERIFY(!s.saveSettings(rules()));
        QCOMPARE(s.settings(), original);
        auto bad = rules();
        bad["minutes"] = 2;
        QVERIFY(!s.saveSettings(bad));
        QCOMPARE(s.settings(), original);
    }
};
QTEST_GUILESS_MAIN(SignalsTests)
#include "signals_controller_tests.moc"
