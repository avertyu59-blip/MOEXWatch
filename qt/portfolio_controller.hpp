#pragma once
#include "app_storage.hpp"
#include "profile_lock.hpp"
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
    Q_PROPERTY(bool cashWritable READ cashWritable NOTIFY changed)
    Q_PROPERTY(bool backupBusy READ backupBusy NOTIFY changed)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QString sortKey READ sortKey NOTIFY changed)
    Q_PROPERTY(RecordModel* watchlist READ watchlist CONSTANT)
    Q_PROPERTY(QString selectedTicker READ selectedTicker WRITE setSelectedTicker NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap analysis READ analysis NOTIFY chartChanged)
    Q_PROPERTY(QVariantList chartCandles READ chartCandles NOTIFY chartChanged)
    Q_PROPERTY(QVariantMap chartLevels READ chartLevels NOTIFY chartChanged)
    Q_PROPERTY(int chartSessions READ chartSessions WRITE setChartSessions NOTIFY viewChanged)
    Q_PROPERTY(bool chartComparison READ chartComparison WRITE setChartComparison NOTIFY viewChanged)
public:
    using Fetch = std::function<moexwatch::MarketRefreshResult(std::vector<moexwatch::Instrument>,std::stop_token)>;
    explicit PortfolioController(std::filesystem::path root = {}, Fetch fetch = {}, QObject* parent = nullptr);
    ~PortfolioController() override;
    RecordModel* positions() { return &positions_; }
    RecordModel* operations() { return &operations_; }
    QVariantMap totals() const { return totals_; }
    QVariantList allocation() const { return allocation_; }
    QStringList tickers() const;
    // Read-only value snapshots for independent analysis modules.
    std::vector<moexwatch::Instrument> instrumentsSnapshot() const { return instruments_; }
    std::vector<moexwatch::Candle> indexSnapshot() const { return index_candles_; }
    std::filesystem::path dataRoot() const { return storage_.root(); }
    bool demo() const { return demo_; }
    moexwatch::PortfolioJournal journalSnapshot() const { return journal_; }
    quint64 journalRevision() const { return journal_revision_; }
    bool commitImportedTransactions(std::vector<moexwatch::PortfolioTransaction> transactions, quint64 expectedRevision);
    bool cashWritable() const { return writable_ && cash_valid_ && !demo_ && !closed_; }
    Q_INVOKABLE bool setFreeCash(QString amount);
    Q_INVOKABLE QVariantMap incomeWithDividends(double receivedNet,bool receiptsValid)const;
    QString status() const { return demo_?QStringLiteral("ДЕМОНСТРАЦИЯ · вымышленные операции и история · ")+status_:status_; }
    QString error() const { return error_; }
    bool writable() const { return writable_; }
    bool busy() const { return active_request_ != 0; }
    bool backupBusy() const { return tasks_.running(moexwatch::TaskKind::Backup); }
    bool dark() const { return dark_; }
    QString sortKey() const { return sort_key_; }
    RecordModel* watchlist() { return &watchlist_; }
    QString selectedTicker() const { return selected_ticker_; }
    void setSelectedTicker(QString ticker);
    QVariantMap analysis() const { return analysis_; }
    QVariantList chartCandles() const { return chart_candles_; }
    QVariantMap chartLevels() const { return chart_levels_; }
    int chartSessions() const { return chart_sessions_; }
    void setChartSessions(int sessions);
    bool chartComparison() const { return chart_comparison_; }
    void setChartComparison(bool comparison);
    Q_INVOKABLE bool addTicker(QString ticker);
    Q_INVOKABLE bool removeTicker(QString ticker);
    Q_INVOKABLE bool setTarget(QString ticker,QString value);
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
    void chartChanged();
    void selectionChanged();
    void viewChanged();
private:
    moexwatch::AppStorage storage_;
    moexwatch::PortfolioJournal journal_;
    quint64 journal_revision_{};
    bool journal_readable_{};
    double free_cash_{};
    std::optional<std::string> cash_snapshot_;
    bool cash_valid_{true};
    QString cash_error_;
    void loadCash();
    std::vector<moexwatch::Instrument> instruments_;
    Fetch fetch_;
    RecordModel positions_, operations_;
    RecordModel watchlist_;
    std::vector<moexwatch::Candle> index_candles_;
    bool index_stale_{true};
    QString index_error_, selected_ticker_;
    int chart_sessions_{66};
    bool chart_comparison_{};
    QVariantMap analysis_, chart_levels_;
    QVariantList chart_candles_;
    QVariantMap totals_;
    QVariantList allocation_;
    QString status_, error_, market_error_, sort_key_{"ticker"};
    bool writable_{true}, dark_{true}, closed_{false}, demo_{};
    std::uint64_t request_sequence_{}, active_request_{};
    moexwatch::TaskGroup tasks_;
    std::shared_ptr<moexwatch::CompletionMailbox> mailbox_{std::make_shared<moexwatch::CompletionMailbox>()};
    std::unique_ptr<moexwatch::ProfileLock> profile_lock_;
    std::unique_ptr<QLockFile> lock_;
    std::unique_ptr<QSettings> settings_;
    QTimer timer_, freshness_timer_;
    bool persist(moexwatch::PortfolioJournal candidate);
    void fail(QString text, bool blockWrites = false);
    void rebuild();
    void rebuildOverview();
    void synchronizePositions();
    void saveViewSettings();
    void drain();
};
