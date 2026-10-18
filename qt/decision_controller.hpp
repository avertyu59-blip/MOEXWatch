#pragma once
#include "portfolio_controller.hpp"
class DecisionController final : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
 Q_PROPERTY(QString error READ error NOTIFY changed)
 Q_PROPERTY(bool writable READ writable NOTIFY changed)
public:
 explicit DecisionController(PortfolioController*,QObject* parent=nullptr);
 QVariantList entries() const { return entries_; }
 QString error() const { return error_; }
 bool writable() const;
 Q_INVOKABLE QVariantMap calculate(QString ticker,QString entry,QString budget,QString lot,QString fee,QString stop,QString target,QString riskBudget) const;
 Q_INVOKABLE bool saveDecision(QVariantMap plan,QString reason,QString exitReason);
 Q_INVOKABLE bool updateDecision(QString id,QString reason,QString exitReason,QString status);
 Q_INVOKABLE bool deleteDecision(QString id);
 signals: void changed();
private:
 bool persist(const QVariantList&);
 void load();
 PortfolioController* portfolio_;
 QString path_,error_;
 bool valid_{true};
 QVariantList entries_;
};
