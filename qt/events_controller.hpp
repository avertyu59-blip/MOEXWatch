#pragma once
#include "portfolio_controller.hpp"
class EventsController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(RecordModel *dividends READ dividends CONSTANT)
  Q_PROPERTY(int dividendCount READ dividendCount NOTIFY changed)
  Q_PROPERTY(RecordModel *corporate READ corporate CONSTANT)
  Q_PROPERTY(bool calendarWritable READ calendarWritable NOTIFY changed)
  Q_PROPERTY(QString ticker READ ticker NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QStringList availableTickers READ availableTickers NOTIFY changed)
  Q_PROPERTY(bool onlyPositions READ onlyPositions WRITE setOnlyPositions NOTIFY
                 changed)
public:
  using Fetch = std::function<moexwatch::DividendFetchResult(std::string,
                                                             std::stop_token)>;
  explicit EventsController(PortfolioController *market,
                            QObject *parent = nullptr);
  EventsController(PortfolioController *market, Fetch fetch,
                   QObject *parent = nullptr);
  ~EventsController() override;
  RecordModel *dividends() { return &rows_; }
  int dividendCount() const { return rows_.rowCount(); }
  RecordModel *corporate() { return &corporate_; }
  bool calendarWritable() const;
  Q_INVOKABLE bool addCorporate(QString date, QString type, QString title,
                                QString source, QString status);
  Q_INVOKABLE bool removeCorporate(QString id);
  QString ticker() const { return ticker_; }
  QString status() const { return status_; }
  QString error() const { return error_; }
  bool busy() const { return active_ != 0; }
  QStringList availableTickers() const;
  bool onlyPositions() const { return only_positions_; }
  void setOnlyPositions(bool value);
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void openSource(QString url);
  Q_INVOKABLE void shutdown();
signals:
  void changed();
  void snapshotChanged();

private:
  PortfolioController *market_;
  RecordModel rows_, corporate_;
  QList<QVariantMap> corporate_records_;
  bool calendar_valid_{true};
  QString calendar_error_;
  void loadCorporate();
  bool saveCorporate(const QList<QVariantMap> &candidate);
  Fetch fetch_;
  moexwatch::TaskGroup tasks_;
  std::shared_ptr<moexwatch::CompletionMailbox> mailbox_{
      std::make_shared<moexwatch::CompletionMailbox>()};
  QTimer timer_;
  QString ticker_, status_, error_;
  moexwatch::DividendSnapshot snapshot_;
  std::uint64_t sequence_{}, active_{};
  bool closed_{}, stale_{true}, only_positions_{};
  void select();
  void rebuild();
  void drain();
};
