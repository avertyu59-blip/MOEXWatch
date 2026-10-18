#include "portfolio_transfer_controller.hpp"
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <QPointer>
#include <QUuid>
#include <QUrl>
#include <set>

namespace {
QString localPath(QString path) {
    const QUrl url(path);
    if (path.startsWith("//") || path.startsWith(QStringLiteral("\\\\"))) return {};
    if (url.isLocalFile()) return url.host().isEmpty()?url.toLocalFile():QString{};
    if (url.isValid() && !url.scheme().isEmpty() && url.scheme().size() != 1) return {};
    return path;
}
QString nativePath(const std::filesystem::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}
bool internalExport(const QString& path, const QString& root) {
    const QFileInfo target(path), directory(target.absolutePath());
    QString resolved = target.exists() ? target.canonicalFilePath() : QDir(directory.canonicalFilePath()).filePath(target.fileName());
    const QString base = QFileInfo(root).canonicalFilePath();
    if (resolved.isEmpty() || base.isEmpty() || directory.canonicalFilePath().isEmpty()) return true;
#ifdef _WIN32
    resolved = resolved.toCaseFolded();
    const QString canonicalRoot = base.toCaseFolded();
#else
    const QString canonicalRoot = base;
#endif
    return resolved == canonicalRoot || resolved.startsWith(canonicalRoot + '/');
}
}
PortfolioTransferController::PortfolioTransferController(PortfolioController* portfolio, QObject* parent)
    : QObject(parent), portfolio_(portfolio), rows_(this) {}
PortfolioTransferController::~PortfolioTransferController() { ++generation_; worker_.request_stop(); if (worker_.joinable()) worker_.join(); }
QVariantMap PortfolioTransferController::summary() const {
    return {{"newCount",new_count_},{"duplicateCount",duplicate_count_},{"errorCount",error_count_},
        {"affectedTickers",affected_},{"newPositionCount",new_positions_},{"canApply",prepared_.has_value() && !busy_ && portfolio_->writable()}};
}
void PortfolioTransferController::cancelImport() {
    ++generation_; worker_.request_stop(); busy_=false; token_.clear(); prepared_.reset();
    preview_={}; rows_.replace({}); new_count_=duplicate_count_=error_count_=new_positions_=0; affected_.clear();
    error_.clear(); status_.clear(); duplicates_=false; emit changed();
}
void PortfolioTransferController::chooseImport() {
    if (!portfolio_->writable()) { error_=QStringLiteral("Профиль доступен только для чтения"); emit changed(); return; }
    const auto path=QFileDialog::getOpenFileName(nullptr,QStringLiteral("Импорт операций"),{},QStringLiteral("CSV UTF-8 (*.csv);;Все файлы (*)"));
    if (!path.isEmpty()) previewFile(path);
}
void PortfolioTransferController::previewFile(QString path) {
    cancelImport();
    if (!portfolio_->writable()) { error_=QStringLiteral("Профиль доступен только для чтения"); emit changed(); return; }
    path=localPath(path);
    const QFileInfo file(path);
    if (path.isEmpty() || !file.isFile() || file.size()>4*1024*1024) { error_=QStringLiteral("Нужен локальный обычный CSV-файл не больше 4 МБ"); emit changed(); return; }
    snapshot_=portfolio_->journalSnapshot(); revision_=portfolio_->journalRevision(); busy_=true; status_=QStringLiteral("Проверка CSV…"); emit changed();
    const auto generation=generation_; const auto existing=snapshot_.transactions;
    QPointer<PortfolioTransferController> self(this);
    worker_=std::jthread([self,path,existing,generation](std::stop_token stop) {
        QFile input(path); moexwatch::ImportPreview preview;
        if (!input.open(QIODevice::ReadOnly)) preview.error="Не удалось открыть CSV";
        else {
            const auto bytes=input.read(4*1024*1024+1);
            if (input.error()!=QFileDevice::NoError) preview.error="Не удалось прочитать CSV";
            else preview=moexwatch::preview_portfolio_csv(std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size())),existing);
        }
        if (stop.stop_requested()) return;
        if (self) QMetaObject::invokeMethod(self,[self,generation,preview=std::move(preview)]() mutable {
            if (!self || generation!=self->generation_) return;
            self->preview_=std::move(preview); self->busy_=false;
            self->token_=QUuid::createUuid().toString(QUuid::WithoutBraces); self->prepare();
        },Qt::QueuedConnection);
    });
}
void PortfolioTransferController::setIncludeDuplicates(bool value) {
    if (duplicates_==value) return;
    duplicates_=value; if (!busy_ && !token_.isEmpty()) prepare(); else emit changed();
}
void PortfolioTransferController::prepare() {
    QList<QVariantMap> rows; new_count_=duplicate_count_=error_count_=new_positions_=0; affected_.clear();
    for (const auto& row:preview_.rows) {
        const bool isNew=row.status==moexwatch::ImportStatus::New;
        if (isNew) ++new_count_; else if(row.status==moexwatch::ImportStatus::Duplicate) ++duplicate_count_; else ++error_count_;
        QVariantMap item{{"line",static_cast<int>(row.line)},{"status",isNew?"new":row.status==moexwatch::ImportStatus::Duplicate?"duplicate":"error"},{"message",QString::fromStdString(row.message)}};
        if (row.transaction) {
            const auto& t=*row.transaction;
            item.insert("date",QString::fromStdString(t.date)); item.insert("ticker",QString::fromStdString(t.ticker));
            item.insert("side",t.side==moexwatch::PortfolioSide::Buy?"BUY":"SELL"); item.insert("quantity",t.quantity); item.insert("price",t.price); item.insert("commission",t.commission);
            item.insert("id",QString::fromStdString(t.id)); item.insert("positionId",QString::fromStdString(t.position_id));
            if (isNew || (duplicates_ && row.status==moexwatch::ImportStatus::Duplicate && !row.identity_duplicate)) affected_.append(QString::fromStdString(t.ticker));
        }
        rows.append(item);
    }
    affected_.removeDuplicates(); affected_.sort(); rows_.replace(rows);
    std::string error;
    prepared_=moexwatch::prepare_portfolio_import(preview_,snapshot_.transactions,duplicates_,error,snapshot_.sequence_watermark);
    error_=QString::fromStdString(error);
    if (prepared_) {
        std::set<std::string> before,after;
        for (const auto& t:snapshot_.transactions) before.insert(t.position_id);
        for (const auto& t:*prepared_) after.insert(t.position_id);
        for (const auto& id:after) if (!before.contains(id)) ++new_positions_;
        status_=QStringLiteral("Предпросмотр готов. Записи ещё не сохранены.");
    } else status_=QStringLiteral("Импорт не применён");
    emit changed();
}
bool PortfolioTransferController::applyImport(QString token) {
    if (busy_ || token.isEmpty() || token!=token_ || !prepared_) { error_=QStringLiteral("Предпросмотр отсутствует или устарел"); emit changed(); return false; }
    if (portfolio_->journalRevision()!=revision_) { prepared_.reset(); token_.clear(); error_=QStringLiteral("Журнал изменился. Повторите предпросмотр."); emit changed(); return false; }
    const auto added=prepared_->size()-snapshot_.transactions.size(); const auto skipped=preview_.rows.size()-added;
    if (!portfolio_->commitImportedTransactions(*prepared_,revision_)) { error_=portfolio_->error(); emit changed(); return false; }
    prepared_.reset(); token_.clear(); status_=QStringLiteral("Добавлено: %1. Пропущено: %2.").arg(added).arg(skipped); error_.clear(); emit changed(); return true;
}
void PortfolioTransferController::chooseExport() {
    const auto path=QFileDialog::getSaveFileName(nullptr,QStringLiteral("Экспорт операций"),QStringLiteral("operations.csv"),QStringLiteral("CSV UTF-8 (*.csv)"));
    if (!path.isEmpty()) exportFile(path);
}
bool PortfolioTransferController::exportFile(QString path) {
    error_.clear(); path=localPath(path);
    if (!portfolio_->writable()) error_=QStringLiteral("Профиль доступен только для чтения");
    else if (portfolio_->journalSnapshot().transactions.empty()) error_=QStringLiteral("Журнал пуст: экспортировать нечего");
    else if (path.isEmpty() || (QFileInfo(path).exists() && !QFileInfo(path).isFile()) || (QFileInfo(path).isSymLink() && QFileInfo(path).canonicalFilePath().isEmpty()) || internalExport(path,nativePath(portfolio_->dataRoot()))) error_=QStringLiteral("Сохраните CSV вне папки данных приложения");
    if (!error_.isEmpty()) { emit changed(); return false; }
    try {
        const auto csv=moexwatch::export_portfolio_csv(portfolio_->journalSnapshot().transactions);
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly) || output.write(csv.data(),static_cast<qint64>(csv.size()))!=static_cast<qint64>(csv.size()) || !output.commit()) error_=QStringLiteral("Не удалось атомарно сохранить CSV");
    } catch (const std::exception& ex) { error_=QString::fromUtf8(ex.what()); }
    if (error_.isEmpty()) status_=QStringLiteral("CSV сохранён с постоянными ID и комиссией");
    emit changed(); return error_.isEmpty();
}
