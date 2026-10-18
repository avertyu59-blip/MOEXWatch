#pragma once
#include "portfolio_controller.hpp"
#include "portfolio_csv.hpp"
#include <thread>

class PortfolioTransferController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(RecordModel* previewRows READ previewRows CONSTANT)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)
    Q_PROPERTY(int newCount READ newCount NOTIFY changed)
    Q_PROPERTY(int duplicateCount READ duplicateCount NOTIFY changed)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY changed)
    Q_PROPERTY(QStringList affectedTickers READ affectedTickers NOTIFY changed)
    Q_PROPERTY(int newPositionCount READ newPositionCount NOTIFY changed)
    Q_PROPERTY(QString previewToken READ previewToken NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool includeDuplicates READ includeDuplicates WRITE setIncludeDuplicates NOTIFY changed)
public:
    explicit PortfolioTransferController(PortfolioController* portfolio, QObject* parent = nullptr);
    ~PortfolioTransferController() override;
    RecordModel* previewRows() { return &rows_; }
    QVariantMap summary() const;
    int newCount() const { return new_count_; }
    int duplicateCount() const { return duplicate_count_; }
    int errorCount() const { return error_count_; }
    QStringList affectedTickers() const { return affected_; }
    int newPositionCount() const { return new_positions_; }
    QString previewToken() const { return token_; }
    QString error() const { return error_; }
    QString status() const { return status_; }
    bool busy() const { return busy_; }
    bool includeDuplicates() const { return duplicates_; }
    void setIncludeDuplicates(bool value);
    Q_INVOKABLE void chooseImport();
    Q_INVOKABLE void previewFile(QString path);
    Q_INVOKABLE bool applyImport(QString token);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void chooseExport();
    Q_INVOKABLE bool exportFile(QString path);
signals:
    void changed();
private:
    void prepare();
    PortfolioController* portfolio_;
    RecordModel rows_;
    moexwatch::ImportPreview preview_;
    moexwatch::PortfolioJournal snapshot_;
    std::optional<std::vector<moexwatch::PortfolioTransaction>> prepared_;
    std::jthread worker_;
    quint64 generation_{}, revision_{};
    int new_count_{}, duplicate_count_{}, error_count_{}, new_positions_{};
    QStringList affected_;
    QString token_, error_, status_;
    bool busy_{}, duplicates_{};
};
