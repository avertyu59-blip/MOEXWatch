#pragma once
#include "app_storage.hpp"
#include "async_tasks.hpp"
#include <QAbstractListModel>
#include <QLockFile>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>
#include <functional>

class RecordModel final : public QAbstractListModel {
    Q_OBJECT
public:
    explicit RecordModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    void replace(QList<QVariantMap> rows);
    QList<QVariantMap> rows() const { return rows_; }
private:
    QList<QVariantMap> rows_;
};

// The UI thread owns all models. Workers receive snapshots and publish values,
// never QObject/Window pointers. Calculations and journal formats stay in C++.
class PortfolioController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(RecordModel* positions READ positions CONSTANT)
    Q_PROPERTY(RecordModel* operations READ operations CONSTANT)
    Q_PROPERTY(QVariantMap totals READ totals NOTIFY changed)
    Q_PROPERTY(QVariantList allocation READ allocation NOTIFY changed)
    Q_PROPERTY(QStringList tickers READ tickers NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool writable READ writable NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool backupBusy READ backupBusy NOTIFY changed)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QString sortKey READ sortKey NOTIFY changed)
public:
    using Fetch = std::function<moexwatch::MarketRefreshResult(std::vector<moexwatch::Instrument>,std::stop_token)>;
    explicit PortfolioController(std::filesystem::path root = {}, Fetch fetch = {}, QObject* parent = nullptr);
    ~PortfolioController() override;
    RecordModel* positions() { return &positions_; }
    RecordModel* operations() { return &operations_; }
    QVariantMap totals() const { return totals_; }
    QVariantList allocation() const { return allocation_; }
    QStringList tickers() const;
    QString status() const { return status_; }
    QString error() const { return error_; }
    bool writable() const { return writable_; }
    bool busy() const { return active_request_ != 0; }
    bool backupBusy() const { return tasks_.running(moexwatch::TaskKind::Backup); }
    bool dark() const { return dark_; }
    QString sortKey() const { return sort_key_; }
    void setDark(bool value);
    Q_INVOKABLE bool addOperation(QString date, QString ticker, int side, QString quantity, QString price, QString fee);
    Q_INVOKABLE bool deleteOperation(QString id);
    Q_INVOKABLE bool saveLevels(QString positionId, QString stop, QString take);
    Q_INVOKABLE QVariantMap position(QString id) const;
    Q_INVOKABLE void sortPositions(QString key);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void backup();
    Q_INVOKABLE void openDataFolder();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void shutdown();
    void loadDemo(); // Explicit demo/smoke mode only; disables journal writes.
signals:
    void changed();
private:
    moexwatch::AppStorage storage_;
    moexwatch::PortfolioJournal journal_;
    std::vector<moexwatch::Instrument> instruments_;
    Fetch fetch_;
    RecordModel positions_, operations_;
    QVariantMap totals_;
    QVariantList allocation_;
    QString status_, error_, sort_key_{"ticker"};
    bool writable_{true}, dark_{true}, closed_{false};
    std::uint64_t request_sequence_{}, active_request_{};
    moexwatch::TaskGroup tasks_;
    std::shared_ptr<moexwatch::CompletionMailbox> mailbox_{std::make_shared<moexwatch::CompletionMailbox>()};
    std::unique_ptr<QLockFile> lock_;
    std::unique_ptr<QSettings> settings_;
    QTimer timer_;
    bool persist(moexwatch::PortfolioJournal candidate);
    void fail(QString text, bool blockWrites = false);
    void rebuild();
    void drain();
};
