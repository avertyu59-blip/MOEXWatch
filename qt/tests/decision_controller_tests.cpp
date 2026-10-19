#include "decision_controller.hpp"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
class DecisionTests: public QObject {
 Q_OBJECT
private slots:
 void externalChangeAndBackup(){QTemporaryDir d;PortfolioController p(d.path().toStdString());DecisionController c(&p);auto plan=c.calculate("SBER","100","10000","10","0","90","120","500");QVERIFY(c.saveDecision(plan,"first",""));QVERIFY(c.saveDecision(plan,"second",""));QVERIFY(QFile::exists(d.filePath("backups/last-good/decision_diary_v1.json")));QFile f(d.filePath("decision_diary_v1.json"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("external");f.close();QVERIFY(!c.saveDecision(plan,"third",""));QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("external"));QCOMPARE(c.entries().size(),2);}
 void arithmetic(){QTemporaryDir d;PortfolioController p(d.path().toStdString());DecisionController c(&p);auto r=c.calculate("SBER","100","10000","10","1","90","120","1000");QVERIFY(r["valid"].toBool());QCOMPARE(r["lots"].toDouble(),8.0);QCOMPARE(r["quantity"].toDouble(),80.0);QCOMPARE(r["total"].toDouble(),8080.0);QCOMPARE(r["loss"].toDouble(),952.0);QCOMPARE(r["profit"].toDouble(),1424.0);}
 void forgedPlan(){QTemporaryDir d;PortfolioController p(d.path().toStdString());DecisionController c(&p);auto plan=c.calculate("SBER","100","10000","10","1","90","120","1000");plan["quantity"]=1e9;plan["profit"]=9e9;plan["valid"]=true;QVERIFY(c.saveDecision(plan,"Тезис",""));auto stored=c.entries().first().toMap()["plan"].toMap();QCOMPARE(stored["quantity"].toDouble(),80.0);QCOMPARE(stored["profit"].toDouble(),1424.0);plan["entry"]="bad";QVERIFY(!c.saveDecision(plan,"Тезис",""));QCOMPARE(c.entries().size(),1);}
 void invalid(){QTemporaryDir d;PortfolioController p(d.path().toStdString());DecisionController c(&p);QVERIFY(!c.calculate("SBER","nan","10000","10","0","90","120","500")["valid"].toBool());QVERIFY(!c.calculate("SBER","100","10000","0","0","90","120","500")["valid"].toBool());QVERIFY(!c.calculate("SBER","100","10000","10","0","100","120","500")["valid"].toBool());QVERIFY(!c.calculate("SBER","100","1","10","0","90","120","500")["valid"].toBool());}
 void roundtrip(){QTemporaryDir d;QString id;{PortfolioController p(d.path().toStdString());DecisionController c(&p);QVERIFY(c.writable());auto r=c.calculate("SBER","100","10000","10","0.05","90","120","500");QVERIFY(c.saveDecision(r,"Рост прибыли","Отмена тезиса"));id=c.entries().first().toMap()["id"].toString();QVERIFY(!id.isEmpty());QVERIFY(c.updateDecision(id,"Новый тезис","Разбор","reviewed"));}{PortfolioController p(d.path().toStdString());DecisionController c(&p);QCOMPARE(c.entries().size(),1);QCOMPARE(c.entries().first().toMap()["id"].toString(),id);QCOMPARE(c.entries().first().toMap()["status"].toString(),QString("reviewed"));QVERIFY(c.deleteDecision(id));QVERIFY(c.entries().isEmpty());}}
 void unicodePath(){QTemporaryDir d;QString dir=d.filePath(QString::fromUtf8("Портфель"));QVERIFY(QDir().mkpath(dir));
#ifdef _WIN32
 std::filesystem::path path(dir.toStdWString());
#else
 std::filesystem::path path(dir.toStdString());
#endif
 {PortfolioController p(path);DecisionController c(&p);auto r=c.calculate("SBER","100","10000","10","0","90","120","500");QVERIFY(c.saveDecision(r,"Тезис",""));}
 {PortfolioController p(path);DecisionController c(&p);QCOMPARE(c.entries().size(),1);}}
 void unknownVersion(){QTemporaryDir d;QFile f(d.filePath("decision_diary_v1.json"));QVERIFY(f.open(QIODevice::WriteOnly));QByteArray source="{\"version\":99,\"entries\":[]}";f.write(source);f.close();PortfolioController p(d.path().toStdString());DecisionController c(&p);QVERIFY(!c.writable());QVERIFY(c.entries().isEmpty());QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),source);}
 void corrupt(){QTemporaryDir d;QFile f(d.filePath("decision_diary_v1.json"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("{bad json");f.close();PortfolioController p(d.path().toStdString());DecisionController c(&p);QVERIFY(!c.writable());QVERIFY(!c.error().isEmpty());auto r=c.calculate("SBER","100","10000","10","0","90","120","500");QVERIFY(!c.saveDecision(r,"Thesis",""));QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("{bad json"));}
};
QTEST_GUILESS_MAIN(DecisionTests)
#include "decision_controller_tests.moc"
