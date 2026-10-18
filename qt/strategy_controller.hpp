#pragma once
#include "backtest_dividends.hpp"
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <functional>
#include <future>
class PortfolioController;
class StrategyController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
public:
  explicit StrategyController(PortfolioController *market,
                              QObject *parent = nullptr);
  ~StrategyController() override;
  bool busy() const { return running_; }
  QString error() const { return error_; }
  QString status() const { return status_; }
  QVariantMap result() const { return result_; }
  Q_INVOKABLE bool run(QString capital, QString commission, QString from,
                       QString to, QVariantMap rules);
  Q_INVOKABLE void clearError();
  void
  setDividendSource(std::function<std::vector<moexwatch::BacktestCashDividend>(
                        std::string_view)>
                        source);
  void invalidateDividendData();
signals:
  void changed();

private:
  struct Outcome {
    QVariantMap result;
    QString error;
  };
  std::function<std::vector<moexwatch::BacktestCashDividend>(std::string_view)>
      dividendSource_;
  QPointer<PortfolioController> market_;
  QTimer timer_;
  std::future<Outcome> pending_;
  bool running_{};
  bool invalidated_{};
  QString error_, status_;
  QVariantMap result_;
  void invalidate();
  void drain();
  bool fail(QString message);
};
