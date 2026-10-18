#include "research_controller.hpp"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <limits>
namespace {
std::filesystem::path rootOf(const QTemporaryDir& dir) {
#ifdef _WIN32
 return dir.path().toStdWString();
#else
 return dir.path().toStdString();
#endif
}
}
class ResearchTests:public QObject {Q_OBJECT
QVariantMap record(QString ticker="SBER") {return {{"ticker",ticker},{"company","Компания"},{"sector","Нефть"},{"standard","IFRS"},{"period","2025"},{"durationMonths",12},{"currency","RUB"},{"unit","million"},{"sourceUrl","https://example.org/report"},{"publicationDate","2026-01-01"},{"checkedDate","2026-01-02"},{"revenue",1000.0},{"netProfit",100.0},{"ebitda",200.0},{"cfo",150.0},{"capex",50.0},{"debt",300.0},{"cash",100.0},{"equity",500.0},{"shares",1000000.0}};}
private slots:
void formulas(){auto m=ResearchController::calculate(record(),1000);QCOMPARE(m["fcf"].toDouble(),100.0);QCOMPARE(m["netDebtEbitda"].toDouble(),1.0);QCOMPARE(m["pe"].toDouble(),10.0);QCOMPARE(m["evEbitda"].toDouble(),6.0);}
void injectedDerivedValuesIgnored(){auto m=record();m.remove("netProfit");m["pe"]=1;m["dividendYield"]=999;auto c=ResearchController::calculate(m,1000);QVERIFY(!c.contains("pe"));QVERIFY(!c.contains("dividendYield"));}
void missingAndZero(){auto m=record();m.remove("cfo");m["netProfit"]=0;auto c=ResearchController::calculate(m,1000);QVERIFY(!c.contains("fcf"));QVERIFY(!c.contains("pe"));QVERIFY(c.contains("netMargin"));QCOMPARE(c["netMargin"].toDouble(),0.0);m["durationMonths"]=6;m["netProfit"]=100;QVERIFY(!ResearchController::calculate(m,1000).contains("pe"));QVERIFY(!ResearchController::calculate(m,1000).contains("netDebtEbitda"));m["sector"]="Банки";QVERIFY(!ResearchController::calculate(m,1000).contains("netDebtEbitda"));}
void validate(){auto m=record();QVERIFY(ResearchController::validate(QJsonObject::fromVariantMap(m)).isEmpty());m["sourceUrl"]="file:///secret";QVERIFY(!ResearchController::validate(QJsonObject::fromVariantMap(m)).isEmpty());}
void periodAndEquity(){auto m=record();m["period"]="2025-Q1";QVERIFY(!ResearchController::validate(QJsonObject::fromVariantMap(m)).isEmpty());m["durationMonths"]=3;QVERIFY(ResearchController::validate(QJsonObject::fromVariantMap(m)).isEmpty());m["period"]="2025";QVERIFY(!ResearchController::validate(QJsonObject::fromVariantMap(m)).isEmpty());m["durationMonths"]=12;m["equity"]=-10;QVERIFY(ResearchController::validate(QJsonObject::fromVariantMap(m)).isEmpty());QVERIFY(!ResearchController::calculate(m,1000).contains("pb"));m["shares"]=1e100;QVERIFY(!ResearchController::validate(QJsonObject::fromVariantMap(m)).isEmpty());QVERIFY(!ResearchController::calculate(record(),std::numeric_limits<double>::infinity()).contains("pe"));}
void persistence(){QTemporaryDir dir;auto p=rootOf(dir);{PortfolioController market(p);ResearchController r(&market);QVERIFY(r.saveRecord(record()));QVERIFY(r.saveRecord(record("GAZP")));r.setComparison({"SBER","GAZP"});QCOMPARE(r.comparison().size(),2);auto m=record("GAZP");m["period"]="2024";QVERIFY(r.saveRecord(m));QVERIFY(!r.comparison()[1].toMap()["comparable"].toBool());} {PortfolioController market(p);ResearchController r(&market);QCOMPARE(r.records().size(),2);QCOMPARE(r.card("SBER")["revenue"].toDouble(),1000.0);}}
void corruptBlocked(){QTemporaryDir dir;QFile f(dir.path()+"/fundamentals_v1.json");QVERIFY(f.open(QIODevice::WriteOnly));f.write("broken");f.close();PortfolioController market(rootOf(dir));ResearchController r(&market);QVERIFY(!r.writable());QVERIFY(!r.saveRecord(record()));QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("broken"));}
void invalidImportDoesNotApply(){QTemporaryDir dir;PortfolioController market(rootOf(dir));ResearchController r(&market);QVERIFY(r.saveRecord(record()));QFile f(dir.path()+"/bad.json");QVERIFY(f.open(QIODevice::WriteOnly));QJsonArray a;a.append(QJsonObject::fromVariantMap(record("GAZP")));a.append(QJsonObject::fromVariantMap(record("GAZP")));f.write(QJsonDocument(QJsonObject{{"version",1},{"records",a}}).toJson());f.close();QVERIFY(!r.previewImport(QUrl::fromLocalFile(f.fileName())));QVERIFY(r.preview().isEmpty());QVERIFY(!r.applyImport());QCOMPARE(r.records().size(),1);QCOMPARE(r.card("SBER")["company"].toString(),QString("Компания"));}
void demoNoWrites(){QTemporaryDir dir;PortfolioController market(rootOf(dir));market.loadDemo();ResearchController r(&market);QVERIFY(!r.writable());QVERIFY(!r.saveRecord(record()));QVERIFY(!QFile::exists(dir.path()+"/fundamentals_v1.json"));}
void importPreview(){QTemporaryDir dir;PortfolioController market(rootOf(dir));ResearchController r(&market);QVERIFY(r.saveRecord(record()));const auto url=QUrl::fromLocalFile(dir.path()+"/export.json");QVERIFY(r.exportFile(url));QVERIFY(r.previewImport(url));QCOMPARE(r.preview().size(),1);QVERIFY(r.applyImport());QCOMPARE(r.records().size(),1);QVERIFY(!r.applyImport());}
};
QTEST_MAIN(ResearchTests)
#include "research_controller_tests.moc"
