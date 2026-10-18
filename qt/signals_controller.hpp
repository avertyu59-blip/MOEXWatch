#pragma once
#include "observation.hpp"
#include "portfolio_controller.hpp"
#include <QTimer>

class SignalsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(RecordModel *screener READ screener CONSTANT)
    Q_PROPERTY(RecordModel *matches READ matches CONSTANT)
    Q_PROPERTY(RecordModel *alerts READ alerts CONSTANT)
    Q_PROPERTY(RecordModel *history READ history CONSTANT)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
    Q_PROPERTY(QVariantMap filters READ filters NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool writable READ writable NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
  public:
    explicit SignalsController(PortfolioController *market, QObject *parent = nullptr);
    RecordModel *screener() { return &screener_; }
    RecordModel *matches() { return &matches_; }
    RecordModel *alerts() { return &alerts_; }
    RecordModel *history() { return &history_; }
    QVariantMap settings() const;
    QVariantMap filters() const { return filters_; }
    QString error() const { return error_; }
    bool writable() const;
    QString summary() const;
    Q_INVOKABLE bool saveSettings(QVariantMap settings);
    Q_INVOKABLE bool setFilters(QVariantMap filters);
    Q_INVOKABLE void sort(QString key, bool ascending = true);
    Q_INVOKABLE bool addAlert(QString ticker, bool below, QString price);
    Q_INVOKABLE bool deleteAlert(QString id);
    Q_INVOKABLE bool setAlertEnabled(QString id, bool enabled);
    Q_INVOKABLE bool clearHistory();
    Q_INVOKABLE void openTicker(QString ticker);
    void rebuild();
  signals:
    void changed();
    void openChart();
    void notification(QString title, QString body);

  private:
    PortfolioController *market_;
    moexwatch::AppStorage storage_;
    moexwatch::SignalSettings settings_;
    moexwatch::ObservationEngine observer_;
    std::vector<moexwatch::SignalEvent> events_;
    RecordModel screener_, matches_, alerts_, history_;
    QVariantMap filters_{{"kind", 0}, {"volumeMin", 0.0}, {"atrMin", 0.0}};
    QString error_, sort_{"ticker"};
    bool ascending_{true}, files_ok_{true}, settings_ok_{true}, rebuilding_{};
    QTimer timer_;
    void refreshModels();
    void timerSettings();
    bool persistAlerts(std::vector<moexwatch::PriceAlert> rules);
};
