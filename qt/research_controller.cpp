#include "research_controller.hpp"
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>
namespace {
const QStringList fields = {"revenue", "netProfit", "ebitda", "cfo",    "capex",
                            "debt",    "cash",      "equity", "shares", "dividendPerShare"};
bool number(const QVariantMap &m, const QString &k) {
    bool ok = false;
    const double n = m.value(k).toDouble(&ok);
    return m.contains(k) && !m.value(k).isNull() && ok && std::isfinite(n);
}
QString local(QUrl u) { return u.isLocalFile() && u.host().isEmpty() ? u.toLocalFile() : QString(); }
bool https(QString s) {
    QUrl u(s);
    return u.isValid() && u.scheme() == "https" && !u.host().isEmpty() && u.userInfo().isEmpty();
}
} // namespace
ResearchController::ResearchController(PortfolioController *m, QObject *p) : QObject(p), market_(m) {
#ifdef _WIN32
    path_ = QString::fromStdWString((m->dataRoot() / "fundamentals_v1.json").wstring());
#else
    path_ = QString::fromStdString((m->dataRoot() / "fundamentals_v1.json").string());
#endif
    file_ = std::make_unique<moexwatch::GuardedFile>(m->dataRoot() / "fundamentals_v1.json");
    load();
    connect(m, &PortfolioController::writableChanged, this, &ResearchController::changed);
    connect(m, &PortfolioController::marketDataChanged, this, &ResearchController::changed);
}
bool ResearchController::writable() const { return market_->writable() && !blocked_; }
QString ResearchController::validate(const QJsonObject &o) {
    if (!QRegularExpression("^[A-Z0-9]{1,16}$").match(o["ticker"].toString()).hasMatch())
        return "Некорректный тикер";
    for (const auto &k : QStringList{"company", "sector", "period"})
        if (o[k].toString().trimmed().isEmpty())
            return "Заполните компанию, сектор и период";
    if (!QRegularExpression("^[12][0-9]{3}(-Q[1-4]|-H[12]|-9M|-FY)?$")
             .match(o["period"].toString())
             .hasMatch())
        return "Период: YYYY, YYYY-Q1, YYYY-H1 или YYYY-9M";
    if (o["company"].toString().size() > 200 || o["sector"].toString().size() > 100 ||
        o["period"].toString().size() > 32 || o["sourceUrl"].toString().size() > 2048)
        return "Слишком длинное поле";
    if (o["standard"] != "IFRS" && o["standard"] != "RAS")
        return "Стандарт: IFRS или RAS";
    if (o["currency"] != "RUB" || o["unit"] != "million")
        return "Суммы должны быть в миллионах рублей";
    if (o["durationMonths"].toInt() != 3 && o["durationMonths"].toInt() != 6 &&
        o["durationMonths"].toInt() != 9 && o["durationMonths"].toInt() != 12)
        return "Длительность периода: 3, 6, 9 или 12 месяцев";
    const auto period = o["period"].toString();
    const auto duration = o["durationMonths"].toInt();
    const int expected = period.contains("-Q")    ? 3
                         : period.contains("-H")  ? 6
                         : period.endsWith("-9M") ? 9
                                                  : 12;
    if (duration != expected)
        return "Длительность не соответствует периоду: Q=3, H=6, 9M=9, год=12 месяцев";
    if (!https(o["sourceUrl"].toString()))
        return "Нужна HTTPS-ссылка на источник";
    for (const auto &k : QStringList{"publicationDate", "checkedDate"}) {
        auto d = QDate::fromString(o[k].toString(), Qt::ISODate);
        if (!d.isValid() || d > QDate::currentDate())
            return "Некорректная дата публикации или проверки";
    }
    if (o["publicationDate"].toString() > o["checkedDate"].toString())
        return "Проверка не может быть раньше публикации";
    for (const auto &k : fields) {
        auto v = o[k];
        if (v.isUndefined() || v.isNull())
            continue;
        if (!v.isDouble() || !std::isfinite(v.toDouble()) || std::abs(v.toDouble()) > 1e18)
            return "Некорректное число: " + k;
        if (k != "netProfit" && k != "cfo" && k != "ebitda" && k != "equity" && v.toDouble() < 0)
            return "Отрицательное значение: " + k;
    }
    return {};
}
QVariantMap ResearchController::calculate(QVariantMap m, double cap) {
    for (const auto &key :
         QStringList{"fcf", "netDebt", "netMargin", "netDebtEbitda", "marketCap", "pe", "pb", "evEbitda",
                     "dividendYield", "netMarginPercent", "dividendYieldPercent"})
        m.remove(key);
    auto ratio = [&](QString out, QString a, QString b) {
        if (number(m, a) && number(m, b) && m[b].toDouble() > 0)
            m[out] = m[a].toDouble() / m[b].toDouble();
    };
    if (number(m, "cfo") && number(m, "capex"))
        m["fcf"] = m["cfo"].toDouble() - m["capex"].toDouble();
    if (number(m, "debt") && number(m, "cash"))
        m["netDebt"] = m["debt"].toDouble() - m["cash"].toDouble();
    ratio("netMargin", "netProfit", "revenue");
    const bool bank = m["sector"].toString().contains("bank", Qt::CaseInsensitive) ||
                      m["sector"].toString().contains("банк", Qt::CaseInsensitive);
    if (!bank && m["durationMonths"].toInt() == 12)
        ratio("netDebtEbitda", "netDebt", "ebitda");
    if (std::isfinite(cap) && cap > 0) {
        m["marketCap"] = cap;
        if (m["durationMonths"].toInt() == 12 && number(m, "netProfit") && m["netProfit"].toDouble() > 0)
            m["pe"] = cap / m["netProfit"].toDouble();
        if (number(m, "equity") && m["equity"].toDouble() > 0)
            m["pb"] = cap / m["equity"].toDouble();
        if (!bank && number(m, "netDebt") && number(m, "ebitda") && m["ebitda"].toDouble() > 0 &&
            m["durationMonths"].toInt() == 12)
            m["evEbitda"] = (cap + m["netDebt"].toDouble()) / m["ebitda"].toDouble();
    }
    if (number(m, "netMargin"))
        m["netMarginPercent"] = m["netMargin"].toDouble() * 100;
    m["bank"] = bank;
    m["metricNote"] =
        bank ? "Для банков долговые мультипликаторы не рассчитываются"
             : (m["durationMonths"].toInt() != 12 ? "P/E и мультипликаторы EBITDA требуют годовой отчётности"
                                                  : "Пропуски и отрицательная прибыль не заменяются нулём");
    const auto date = QDate::fromString(m["checkedDate"].toString(), Qt::ISODate);
    m["stale"] = !date.isValid() || date.daysTo(QDate::currentDate()) > 90;
    for (auto it = m.begin(); it != m.end();) {
        if (it.value().metaType().id() == QMetaType::Double && !std::isfinite(it.value().toDouble()))
            it = m.erase(it);
        else
            ++it;
    }
    return m;
}
QVariantMap ResearchController::card(QString ticker) const {
    for (auto &o : data_)
        if (o["ticker"].toString() == ticker) {
            auto m = o.toVariantMap();
            double price = 0;
            for (const auto &i : market_->instrumentsSnapshot())
                if (QString::fromStdString(i.ticker) == ticker) {
                    auto q = i.quote.last;
                    if (q)
                        price = *q;
                }
            double cap = number(m, "shares") ? price * m["shares"].toDouble() / 1e6 : 0;
            m = calculate(m, cap);
            if (price > 0 && number(m, "dividendPerShare"))
                m["dividendYield"] = m["dividendPerShare"].toDouble() / price;
            if (number(m, "dividendYield"))
                m["dividendYieldPercent"] = m["dividendYield"].toDouble() * 100;
            m["valuationNote"] = "Оценка по последней доступной цене; проверьте её свежесть в обзоре";
            return m;
        }
    return {};
}
QVariantList ResearchController::records() const {
    QVariantList r;
    for (auto &o : data_)
        r << card(o["ticker"].toString());
    return r;
}
QVariantList ResearchController::comparison() const {
    QVariantList r;
    QString period, standard;
    int duration = 0;
    for (auto &t : selected_) {
        auto m = card(t);
        if (m.isEmpty())
            continue;
        if (period.isEmpty()) {
            period = m["period"].toString();
            standard = m["standard"].toString();
            duration = m["durationMonths"].toInt();
        }
        const bool ok =
            m["period"] == period && m["standard"] == standard && m["durationMonths"].toInt() == duration;
        m["comparable"] = ok;
        m["comparisonNote"] = ok ? "Сопоставимый период и стандарт"
                                 : "Другой период или стандарт — прямое сравнение некорректно";
        r << m;
    }
    return r;
}
void ResearchController::setComparison(QStringList s) {
    s.removeDuplicates();
    selected_ = s.mid(0, 4);
    emit changed();
}
bool ResearchController::persist(const QList<QJsonObject> &rows) {
    if (rows.size() > 500) {
        error_ = "Лимит 500 компаний";
        emit changed();
        return false;
    }
    if (!writable()) {
        error_ = "Запись недоступна: демонстрация, блокировка профиля или повреждение файла";
        emit changed();
        return false;
    }
    QJsonArray a;
    for (auto &o : rows)
        a.append(o);
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"records", a}}).toJson();
    if (bytes.size() > 4 * 1024 * 1024) {
        error_ = "Лимит файла 4 МБ";
        emit changed();
        return false;
    }
    try {
        file_->write(bytes.toStdString());
    } catch (const std::exception &ex) {
        if (dynamic_cast<const moexwatch::FileConflict *>(&ex))
            blocked_ = true;
        error_ = QString::fromUtf8(ex.what());
        emit changed();
        return false;
    }
    data_ = rows;
    error_.clear();
    emit changed();
    return true;
}
void ResearchController::load() {
    std::optional<std::string> source;
    try {
        source = file_->read();
    } catch (const std::exception &ex) {
        blocked_ = true;
        error_ = QString::fromUtf8(ex.what());
        return;
    }
    if (!source)
        return;
    QJsonParseError e;
    auto d = QJsonDocument::fromJson(QByteArray::fromStdString(*source), &e);
    auto o = d.object();
    if (e.error != QJsonParseError::NoError || (!o["version"].isDouble() || o["version"].toDouble() != 1) ||
        !o["records"].isArray() || o["records"].toArray().size() > 500) {
        blocked_ = true;
        error_ = "Повреждённый или неподдерживаемый файл отчётности; запись заблокирована";
        return;
    }
    QSet<QString> seen;
    for (auto v : o["records"].toArray()) {
        auto row = v.toObject();
        if (!v.isObject() || !validate(row).isEmpty() || seen.contains(row["ticker"].toString())) {
            data_.clear();
            blocked_ = true;
            error_ = "Некорректная запись отчётности; исходный файл сохранён";
            return;
        }
        seen.insert(row["ticker"].toString());
        data_ << row;
    }
}
bool ResearchController::saveRecord(QVariantMap m) {
    auto o = QJsonObject::fromVariantMap(m);
    for (auto it = o.begin(); it != o.end();) {
        const auto key = it.key();
        if (!fields.contains(key) &&
            !QStringList{"ticker", "company", "sector", "standard", "period", "durationMonths", "currency",
                         "unit", "sourceUrl", "publicationDate", "checkedDate"}
                 .contains(key))
            it = o.erase(it);
        else
            ++it;
    }
    error_ = validate(o);
    if (!error_.isEmpty()) {
        emit changed();
        return false;
    }
    auto rows = data_;
    bool found = false;
    for (auto &row : rows)
        if (row["ticker"] == o["ticker"]) {
            row = o;
            found = true;
            break;
        }
    if (!found)
        rows << o;
    return persist(rows);
}
bool ResearchController::previewImport(QUrl u) {
    pending_.clear();
    preview_.clear();
    QFile f(local(u));
    if (!f.open(QIODevice::ReadOnly) || f.size() > 4 * 1024 * 1024) {
        error_ = "Не удалось открыть JSON (лимит 4 МБ)";
        emit changed();
        return false;
    }
    QJsonParseError e;
    auto d = QJsonDocument::fromJson(f.readAll(), &e);
    auto o = d.object();
    if (e.error != QJsonParseError::NoError || (!o["version"].isDouble() || o["version"].toDouble() != 1) ||
        !o["records"].isArray() || o["records"].toArray().size() > 500) {
        error_ = "Некорректный формат версии 1";
        emit changed();
        return false;
    }
    QSet<QString> seen;
    for (auto v : o["records"].toArray()) {
        auto row = v.toObject();
        error_ = validate(row);
        if (!v.isObject() || !error_.isEmpty() || seen.contains(row["ticker"].toString())) {
            error_ = "Импорт отменён: ошибка или дубликат. " + error_;
            pending_.clear();
            preview_.clear();
            emit changed();
            return false;
        }
        seen.insert(row["ticker"].toString());
        pending_ << row;
        preview_ << calculate(row.toVariantMap());
    }
    error_.clear();
    emit changed();
    return true;
}
bool ResearchController::applyImport() {
    if (pending_.isEmpty()) {
        error_ = "Сначала откройте непустой файл и проверьте предпросмотр";
        emit changed();
        return false;
    }
    auto rows = data_;
    for (auto &o : pending_) {
        bool found = false;
        for (auto &r : rows)
            if (r["ticker"] == o["ticker"]) {
                r = o;
                found = true;
                break;
            }
        if (!found)
            rows << o;
    }
    if (!persist(rows))
        return false;
    pending_.clear();
    preview_.clear();
    emit changed();
    return true;
}
bool ResearchController::exportFile(QUrl u) {
    if (local(u).isEmpty()) {
        error_ = "Выберите локальный файл";
        emit changed();
        return false;
    }
    const QFileInfo target(local(u)), parent(target.absolutePath()), profile(path_);
    const auto canonicalRoot = QFileInfo(profile.absolutePath()).canonicalFilePath();
    const auto canonicalParent = parent.canonicalFilePath();
    auto resolved =
        target.exists() ? target.canonicalFilePath() : QDir(canonicalParent).filePath(target.fileName());
    auto base = canonicalRoot;
#ifdef _WIN32
    resolved = resolved.toCaseFolded();
    base = base.toCaseFolded();
#endif
    if (target.isSymLink() || (target.exists() && !target.isFile()) || canonicalParent.isEmpty() ||
        base.isEmpty() || resolved == base || resolved.startsWith(base + '/')) {
        error_ = "Экспортируйте в обычный файл вне каталога профиля, чтобы не перезаписать рабочие данные";
        emit changed();
        return false;
    }
    QJsonArray a;
    for (auto &o : data_)
        a << o;
    QSaveFile f(local(u));
    auto b = QJsonDocument(QJsonObject{{"version", 1}, {"records", a}}).toJson();
    if (!f.open(QIODevice::WriteOnly) || f.write(b) != b.size() || !f.commit()) {
        error_ = "Ошибка экспорта";
        emit changed();
        return false;
    }
    error_.clear();
    emit changed();
    return true;
}
bool ResearchController::openSource(QString ticker) {
    auto m = card(ticker);
    auto s = m["sourceUrl"].toString();
    return https(s) && QDesktopServices::openUrl(QUrl(s));
}
