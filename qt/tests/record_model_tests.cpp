#include "portfolio_controller.hpp"
#include "signals_controller.hpp"
#include "workspace_settings.hpp"
#include <QAbstractItemModelTester>
#include <QFile>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
class ModelTests : public QObject {
    Q_OBJECT
  private slots:
    void identityThroughUpdatesAndSort() {
        RecordModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.replace({{{"id", "a"}, {"price", 10}}, {{"id", "b"}, {"price", 20}}, {{"id", "c"}, {"price", 30}}});
        QPersistentModelIndex selected = m.index(1, 0);
        QItemSelectionModel selection(&m);
        selection.setCurrentIndex(selected, QItemSelectionModel::ClearAndSelect);
        QSignalSpy reset(&m, &QAbstractItemModel::modelReset), changed(&m, &QAbstractItemModel::dataChanged);
        m.replace({{{"id", "c"}, {"price", 30}}, {{"id", "b"}, {"price", 21}}, {{"id", "a"}, {"price", 10}}});
        QVERIFY(selected.isValid());
        QCOMPARE(selected.data(Qt::UserRole + 1).toMap()["id"].toString(), QString("b"));
        QCOMPARE(selection.currentIndex().data(Qt::UserRole + 1).toMap()["id"].toString(), QString("b"));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(reset.size(), 0);
        m.replace(m.rows());
        QCOMPARE(changed.size(), 1);
        m.replace({{{"id", "new"}, {"price", 1}}, {{"id", "b"}, {"price", 22}}});
        QVERIFY(selected.isValid());
        QCOMPARE(selected.row(), 1);
        m.replace({{{"id", "new"}, {"price", 1}}});
        QVERIFY(!selected.isValid());
        QCOMPARE(reset.size(), 0);
    }
    void unkeyedRowsRemainValid() {
        RecordModel m;
        QAbstractItemModelTester t(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.replace({{{"text", "a"}}, {{"text", "b"}}});
        m.replace({{{"text", "c"}}});
        m.replace({});
        QCOMPARE(m.rowCount(), 0);
    }
    void visualChangesDoNotRecomputeSignals() {
        QTemporaryDir d;
        PortfolioController c(d.path().toStdString());
        QVERIFY(c.addTicker("SBER"));
        SignalsController watcher(&c);
        QSignalSpy market(&c, &PortfolioController::marketDataChanged),
            rules(&watcher, &SignalsController::changed), journal(&c, &PortfolioController::journalChanged);
        c.setDark(!c.dark());
        QVERIFY(c.setFreeCash("100"));
        QCOMPARE(market.size(), 0);
        QCOMPARE(rules.size(), 0);
        QCOMPARE(journal.size(), 0);
        QVERIFY(c.setTarget("SBER", "200"));
        QCOMPARE(market.size(), 1);
        QCOMPARE(rules.size(), 1);
        QVERIFY(c.addOperation("2026-01-01", "SBER", 0, "1", "100", "0"));
        QCOMPARE(journal.size(), 1);
    }
    void futureSettingsNeverOverwritten() {
        QTemporaryDir d;
        const auto path = d.filePath("settings.ini");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        const QByteArray bytes = "[format]\nversion=99\n[appearance]\ndark=false\n";
        f.write(bytes);
        f.close();
        WorkspaceSettings s;
        s.open(path);
        s.state().dark = false;
        QVERIFY(!s.save());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), bytes);
    }
};
QTEST_GUILESS_MAIN(ModelTests)
#include "record_model_tests.moc"
