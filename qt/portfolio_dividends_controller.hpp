#pragma once
#include "backtest_dividends.hpp"
#include "dividend_income.hpp"
#include "portfolio_controller.hpp"
class PortfolioDividendsController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(RecordModel *events READ events CONSTANT)
  Q_PROPERTY(RecordModel *receipts READ receipts CONSTANT)
  Q_PROPERTY(QVariantMap totals READ totals NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(bool writable READ writable NOTIFY changed)
public:
  explicit PortfolioDividendsController(PortfolioController *market,
                                        QObject *parent = nullptr);
  RecordModel *events() { return &events_; }
  RecordModel *receipts() { return &receipts_; }
  QVariantMap totals() const { return totals_; }
  QString error() const { return error_; }
  bool writable() const {
    return valid_ && market_->writable() && !market_->demo();
  }
  Q_INVOKABLE void reload();
  Q_INVOKABLE QVariantMap preview(QString eventKey) const;
  Q_INVOKABLE bool confirm(QString eventKey, QString paymentDate, QString gross,
                           QString tax, QString reference,
                           QString previewToken);
  Q_INVOKABLE bool removeReceipt(QString id);
  std::vector<moexwatch::BacktestCashDividend>
  backtestDividends(const std::string &ticker) const;
signals:
  void changed();
  void incomeChanged();

private:
  struct Candidate {
    std::string ticker;
    moexwatch::DividendEvent event;
    moexwatch::DividendEntitlement entitlement;
    QString token;
    QString checkedAt;
  };
  PortfolioController *market_;
  RecordModel events_, receipts_;
  QVariantMap totals_;
  QString error_;
  mutable QString cache_error_;
  QByteArray persisted_hash_;
  bool valid_{true};
  QList<QVariantMap> saved_;
  std::map<QString, Candidate> candidates_;
  void load();
  void rebuild();
  bool save(const QList<QVariantMap> &candidate);
  std::map<QString, Candidate> calculateCandidates() const;
};
