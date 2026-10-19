#pragma once
#include "portfolio_controller.hpp"
#include "guarded_file.hpp"
#include <QJsonObject>
#include <QUrl>
class ResearchController final:public QObject {
 Q_OBJECT
 Q_PROPERTY(QString error READ error NOTIFY changed)
 Q_PROPERTY(QVariantList records READ records NOTIFY changed)
 Q_PROPERTY(QVariantList preview READ preview NOTIFY changed)
 Q_PROPERTY(QVariantList comparison READ comparison NOTIFY changed)
 Q_PROPERTY(bool writable READ writable NOTIFY changed)
public:
 explicit ResearchController(PortfolioController*,QObject* parent=nullptr);
 QString error()const{return error_;} QVariantList records()const; QVariantList preview()const{return preview_;} QVariantList comparison()const;
 bool writable()const;
 Q_INVOKABLE QVariantMap card(QString ticker)const;
 Q_INVOKABLE bool saveRecord(QVariantMap);
 Q_INVOKABLE bool previewImport(QUrl);
 Q_INVOKABLE bool applyImport();
 Q_INVOKABLE bool exportFile(QUrl);
 Q_INVOKABLE void setComparison(QStringList);
 Q_INVOKABLE bool openSource(QString);
 static QString validate(const QJsonObject&);
 static QVariantMap calculate(QVariantMap,double marketCapitalization=0);
signals:void changed();
private:
 std::unique_ptr<moexwatch::GuardedFile> file_;
 PortfolioController* market_; QString path_,error_;QList<QJsonObject> data_,pending_; QVariantList preview_;QStringList selected_;bool blocked_=false;
 bool persist(const QList<QJsonObject>&);void load();
};
