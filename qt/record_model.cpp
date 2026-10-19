#include "portfolio_controller.hpp"
#include <QJsonDocument>
#include <QSet>
namespace {
QStringList keys(const QList<QVariantMap> &rows, const QString &field) {
    QStringList result;
    QSet<QString> seen;
    for (const auto &row : rows) {
        const auto key = row.value(field).toString();
        if (key.isEmpty() || seen.contains(key))
            return {};
        seen.insert(key);
        result.push_back(key);
    }
    return result;
}
} // namespace
int RecordModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}
QVariant RecordModel::data(const QModelIndex &i, int role) const {
    return i.isValid() && i.column() == 0 && i.row() >= 0 && i.row() < rows_.size() &&
                   role == Qt::UserRole + 1
               ? QVariant(rows_[i.row()])
               : QVariant{};
}
QHash<int, QByteArray> RecordModel::roleNames() const { return {{Qt::UserRole + 1, "record"}}; }
void RecordModel::replace(QList<QVariantMap> next) {
    if (next == rows_)
        return;
    QStringList oldKeys, newKeys;
    for (const auto &field : {QStringLiteral("id"), QStringLiteral("key"), QStringLiteral("ticker")}) {
        auto a = keys(rows_, field), b = keys(next, field);
        if (a.size() == rows_.size() && b.size() == next.size()) {
            oldKeys = std::move(a);
            newKeys = std::move(b);
            break;
        }
    }
    // Rows without a unique business key have no identity to preserve. Remove/insert
    // only those rows; never invalidate every persistent index with a model reset.
    if (oldKeys.size() != rows_.size() || newKeys.size() != next.size()) {
        if (!rows_.isEmpty()) {
            beginRemoveRows({}, 0, static_cast<int>(rows_.size()) - 1);
            rows_.clear();
            endRemoveRows();
        }
        if (!next.isEmpty()) {
            beginInsertRows({}, 0, static_cast<int>(next.size()) - 1);
            rows_ = std::move(next);
            endInsertRows();
        }
        return;
    }
    const QSet<QString> wanted(newKeys.begin(), newKeys.end());
    for (int i = static_cast<int>(oldKeys.size()) - 1; i >= 0; --i)
        if (!wanted.contains(oldKeys[i])) {
            beginRemoveRows({}, i, i);
            rows_.removeAt(i);
            oldKeys.removeAt(i);
            endRemoveRows();
        }
    for (int i = 0; i < next.size(); ++i) {
        const int from = static_cast<int>(oldKeys.indexOf(newKeys[i], i));
        if (from < 0) {
            beginInsertRows({}, i, i);
            rows_.insert(i, next[i]);
            oldKeys.insert(i, newKeys[i]);
            endInsertRows();
        } else {
            if (from != i) {
                beginMoveRows({}, from, from, {}, i);
                rows_.move(from, i);
                oldKeys.move(from, i);
                endMoveRows();
            }
            if (rows_[i] != next[i]) {
                rows_[i] = std::move(next[i]);
                emit dataChanged(index(i, 0), index(i, 0), {Qt::UserRole + 1});
            }
        }
    }
}
