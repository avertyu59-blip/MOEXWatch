#include "portfolio_controller.hpp"
#include "observation.hpp"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDateTime>
#include <QDir>
#include <future>
using namespace moexwatch;
class PortfolioTests:public QObject {
    Q_OBJECT
private slots:
    void journalRoundTripAndLevels() {
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto root=std::filesystem::path(dir.path().toStdString());
        QString id;
        {
            PortfolioController c(root);QVERIFY(c.writable());
            QVERIFY(c.addOperation("2026-10-01","sber",0,"10","100,50","5"));
            QCOMPARE(c.positions()->rowCount(),1);auto row=c.positions()->rows().first();id=row["id"].toString();QVERIFY(!id.isEmpty());
            QVERIFY(c.saveLevels(id,"90","130"));
            c.sortPositions("profit");QCOMPARE(c.position(id)["stop"].toString(),QString("90"));
            QVERIFY(!c.addOperation("2026-10-02","SBER",1,"11","120","0"));
            QCOMPARE(c.operations()->rowCount(),1);
            QVERIFY(!c.addOperation("2026-02-30","SBER",0,"1","100","0"));
            QVERIFY(!c.addOperation("2026-10-02","SBER",0,"nan","100","0"));
            QVERIFY(c.addOperation("2026-10-02","SBER",1,"10","120","0"));
            QCOMPARE(c.positions()->rowCount(),0);
            QVERIFY(c.addOperation("2026-10-02","SBER",0,"1","100","0"));
            const auto second=c.positions()->rows().first()["id"].toString();QVERIFY(second!=id);QCOMPARE(c.position(second)["stop"].toString(),QString{});
            const auto opening=c.operations()->rows().last()["id"].toString();
            QVERIFY(!c.deleteOperation(opening));QCOMPARE(c.operations()->rowCount(),3);
        }
        PortfolioController restored(root);QCOMPARE(restored.operations()->rowCount(),3);QCOMPARE(restored.positions()->rowCount(),1);
        const auto journal=AppStorage(root).load_portfolio();QCOMPARE(journal.levels.size(),std::size_t(1));QCOMPARE(QString::fromStdString(journal.levels[0].position_id),id);
    }
    void settingsFailureDoesNotBlockJournal() {
        QTemporaryDir dir;const auto root=std::filesystem::path(dir.path().toStdString());
        QDir(dir.path()).mkdir("qt-workspace.ini");
        PortfolioController c(root);c.setDark(false);QVERIFY(c.writable());
        QVERIFY(c.addOperation("2026-10-01","SBER",0,"1","100","0"));
    }
    void corruptJournalIsPreserved() {
        QTemporaryDir dir;const auto root=std::filesystem::path(dir.path().toStdString());
        {PortfolioController c(root);QVERIFY(c.addOperation("2026-10-01","SBER",0,"1","100","0"));}
        const auto path=dir.path()+"/portfolio_journal_v2.csv";QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("corrupt");f.close();
        PortfolioController c(root);QVERIFY(!c.writable());QVERIFY(!c.addOperation("2026-10-01","SBER",0,"1","100","0"));
        QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("corrupt"));
    }
    void cancelAndShutdownKeepSnapshot() {
        QTemporaryDir dir;std::promise<void> started;std::atomic<bool> stopped{};
        PortfolioController c(dir.path().toStdString(),[&](auto,std::stop_token stop)->MarketRefreshResult {
            started.set_value();cancellable_wait(stop,std::chrono::seconds(10));stopped=true;check_cancelled(stop);return {};
        });
        QVERIFY(c.addOperation("2026-10-01","SBER",0,"10","100","0"));
        c.refresh();QVERIFY(c.busy());started.get_future().wait();c.cancel();QVERIFY(!c.busy());
        QElapsedTimer timer;timer.start();c.shutdown();QVERIFY(stopped);QVERIFY(timer.elapsed()<2000);QCOMPARE(c.operations()->rowCount(),1);
    }
    void quoteApplyAndIdentity() {
        QTemporaryDir dir;
        PortfolioController c(dir.path().toStdString(),[](auto items,std::stop_token)->MarketRefreshResult {
            const auto stamp=observation_time(QDateTime::currentSecsSinceEpoch());
            for(auto& i:items){i.quote={i.ticker,120,0,stamp.substr(11),stamp};i.quote_stale=false;}
            return {false,std::move(items),{}, {}};
        });
        QVERIFY(c.addOperation("2026-10-01","SBER",0,"10","100","0"));
        const auto id=c.positions()->rows().first()["id"].toString();c.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),3000);
        QCOMPARE(c.positions()->rows().first()["id"].toString(),id);
        QVERIFY(c.totals()["fresh"].toBool());QCOMPARE(c.allocation().size(),1);
        QCOMPARE(c.positions()->rows().first()["profitValue"].toDouble(),200.0);
    }
};
QTEST_GUILESS_MAIN(PortfolioTests)
#include "portfolio_controller_tests.moc"
