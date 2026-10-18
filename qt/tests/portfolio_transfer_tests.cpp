#include "portfolio_transfer_controller.hpp"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
using namespace moexwatch;
class PortfolioTransferTests : public QObject {
    Q_OBJECT
    const QByteArray header="date;ticker;side;quantity;price;commission\n";
    void write(const QString& path,const QByteArray& data) { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data),qint64(data.size())); }
private slots:
    void roundTripUnicodeAndDuplicates() {
        QTemporaryDir root,files; const auto path=files.path()+QString::fromUtf8("/операции.csv");
        write(path,header+"2026-01-01;SBER;BUY;2;100;1\n");
        QString id;
        {
            PortfolioController p(std::filesystem::path(root.path().toStdString())); PortfolioTransferController t(&p);
            t.previewFile(path); QTRY_VERIFY(!t.busy()); QCOMPARE(t.newCount(),1); QVERIFY(t.summary()["canApply"].toBool()); QVERIFY(t.applyImport(t.previewToken()));
            id=QString::fromStdString(p.journalSnapshot().transactions.front().id); QVERIFY(!id.isEmpty());
            QVERIFY(!t.exportFile(root.path()+"/journal.csv")); QVERIFY(t.exportFile(files.path()+"/export.csv"));
            QFile exported(files.path()+"/export.csv"); QVERIFY(exported.open(QIODevice::ReadOnly)); QVERIFY(exported.readAll().startsWith("\xEF\xBB\xBF"));
        }
        PortfolioController p(std::filesystem::path(root.path().toStdString())); PortfolioTransferController t(&p);
        QCOMPARE(QString::fromStdString(p.journalSnapshot().transactions.front().id),id);
        t.previewFile(files.path()+"/export.csv"); QTRY_VERIFY(!t.busy()); QCOMPARE(t.duplicateCount(),1); t.setIncludeDuplicates(true); QVERIFY(!t.summary()["canApply"].toBool());
        t.previewFile(path); QTRY_VERIFY(!t.busy()); QCOMPARE(t.duplicateCount(),1); t.setIncludeDuplicates(true); QVERIFY(t.applyImport(t.previewToken())); QCOMPARE(p.operations()->rowCount(),2);
    }
    void errorsCancellationStaleAndSize() {
        QTemporaryDir root,files; PortfolioController p(std::filesystem::path(root.path().toStdString())); PortfolioTransferController t(&p);
        const auto path=files.path()+"/input.csv";
        write(path,header+"bad;SBER;BUY;1;100;0\n"); t.previewFile(path); QTRY_VERIFY(!t.busy()); QCOMPARE(t.errorCount(),1); QVERIFY(!t.applyImport(t.previewToken()));
        write(path,header+"2026-01-01;SBER;BUY;1;100;0\n"); t.previewFile(path); t.cancelImport(); QTest::qWait(30); QVERIFY(t.previewToken().isEmpty()); QCOMPARE(t.previewRows()->rowCount(),0);
        t.previewFile(path); QTRY_VERIFY(!t.busy()); const auto token=t.previewToken(); QVERIFY(p.addOperation("2026-01-02","GAZP",0,"1","100","0")); QVERIFY(!t.applyImport(token)); QVERIFY(t.previewToken().isEmpty());
        write(path,QByteArray(4*1024*1024+1,'x')); t.previewFile(path); QVERIFY(!t.busy()); QVERIFY(!t.error().isEmpty());
        t.previewFile("https://example.com/operations.csv"); QVERIFY(!t.error().isEmpty());
    }
    void exportSafetyAndReadOnly() {
        QTemporaryDir root,files; PortfolioController p(std::filesystem::path(root.path().toStdString())); PortfolioTransferController t(&p);
        QVERIFY(!t.exportFile(files.path()+"/empty.csv")); QVERIFY(p.addOperation("2026-01-01","SBER",0,"1","100","0"));
        QVERIFY(!t.exportFile(root.path()+"/../"+QFileInfo(root.path()).fileName()+"/blocked.csv"));
#ifndef _WIN32
        const auto link=files.path()+"/linked"; QVERIFY(QFile::link(root.path(),link)); QVERIFY(!t.exportFile(link+"/blocked.csv"));
#endif
        PortfolioController locked(std::filesystem::path(root.path().toStdString())); PortfolioTransferController readOnly(&locked); QVERIFY(!locked.writable()); QVERIFY(!readOnly.exportFile(files.path()+"/no.csv"));
    }
    void anonymousDuplicateIgnoresCommissionButIdConflictDoesNot() {
        PortfolioTransaction a{"2026-01-01","SBER",PortfolioSide::Buy,1,100,1};
        auto existing=identify_portfolio({a});
        auto preview=preview_portfolio_csv((header+"2026-01-01;SBER;BUY;1;100;2\n").toStdString(),existing);
        QCOMPARE(preview.rows.front().status,ImportStatus::Duplicate);
        auto changed=existing; changed.front().commission=2;
        preview=preview_portfolio_csv(export_portfolio_csv(changed),existing);
        QCOMPARE(preview.rows.front().status,ImportStatus::Error);
    }
};
QTEST_MAIN(PortfolioTransferTests)
#include "portfolio_transfer_tests.moc"
