#include "decision_controller.hpp"
#include "purchase_plan.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <cmath>
#include <limits>
namespace {
bool number(QString s, double &out) {
    s.replace(',', '.');
    bool ok = false;
    out = s.toDouble(&ok);
    return ok && std::isfinite(out);
}
} // namespace
DecisionController::DecisionController(PortfolioController *p, QObject *parent)
    : QObject(parent), portfolio_(p) {
#ifdef _WIN32
    path_ = QDir(QString::fromStdWString(p->dataRoot().wstring())).filePath("decision_diary_v1.json");
#else
    path_ = QDir(QString::fromStdString(p->dataRoot().string())).filePath("decision_diary_v1.json");
#endif
    file_ = std::make_unique<moexwatch::GuardedFile>(p->dataRoot() / "decision_diary_v1.json");
    load();
    connect(p, &PortfolioController::writableChanged, this, &DecisionController::changed);
}
bool DecisionController::writable() const { return valid_ && portfolio_->writable(); }
QVariantMap DecisionController::calculate(QString ticker, QString entry, QString budget, QString lot,
                                          QString fee, QString stop, QString target,
                                          QString riskBudget) const {
    QVariantMap r;
    auto fail = [&](QString e) {
        r["valid"] = false;
        r["error"] = e;
        return r;
    };
    ticker = ticker.trimmed().toUpper();
    if (!QRegularExpression("^[A-Z0-9][A-Z0-9._-]{0,31}$").match(ticker).hasMatch())
        return fail("Укажите корректный тикер");
    double e = 0, b = 0, l = 0, f = 0, s = 0, t = 0, rb = 0;
    if (!number(entry, e) || !number(budget, b) || !number(lot, l) || !number(fee, f) || !number(stop, s) ||
        !number(target, t) || !number(riskBudget, rb))
        return fail("Все поля должны содержать конечные числа");
    if (e <= 0 || b <= 0 || l < 1 || std::floor(l) != l || l > 1e9 || f < 0 || f >= 100 || s <= 0 || s >= e ||
        t <= e || rb <= 0)
        return fail(
            "Проверьте: бюджет и риск > 0, лот целый, стоп ниже входа, цель выше входа, комиссия 0–100%");
    const auto plan = moexwatch::calculate_purchase_plan({ticker.toStdString(), e, b, l, f, s, t, rb});
    if (!plan)
        return fail("Бюджета или лимита риска недостаточно либо значения выходят за диапазон");
    const auto &p = *plan;
    r = {{"valid", true},
         {"ticker", ticker},
         {"entry", e},
         {"budget", b},
         {"lotSize", l},
         {"feePercent", f},
         {"stop", s},
         {"target", t},
         {"riskBudget", rb},
         {"lots", p.lots},
         {"quantity", p.quantity},
         {"cost", p.cost},
         {"commission", p.commission},
         {"total", p.total},
         {"loss", p.loss},
         {"profit", p.profit},
         {"ratio", p.ratio},
         {"riskPercent", p.risk_percent}};
    return r;
}
void DecisionController::load() {
    std::optional<std::string> source;
    try {
        source = file_->read();
    } catch (const std::exception &ex) {
        valid_ = false;
        error_ = QString::fromUtf8(ex.what());
        return;
    }
    if (!source)
        return;
    auto bad = [&] {
        valid_ = false;
        error_ =
            "Дневник повреждён или имеет неподдерживаемую версию. Исходный файл сохранён; запись отключена.";
    };

    QJsonParseError pe;
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(*source), &pe);
    auto obj = doc.object();
    if (pe.error != QJsonParseError::NoError || !doc.isObject() ||
        (!obj.value("version").isDouble() || obj.value("version").toDouble() != 1) ||
        !obj.value("entries").isArray()) {
        bad();
        return;
    }
    auto a = obj.value("entries").toArray();
    if (a.size() > 10000) {
        bad();
        return;
    }
    QSet<QString> ids;
    QVariantList loaded;
    for (auto v : a) {
        auto m = v.toObject().toVariantMap();
        QString id = m.value("id").toString(), status = m.value("status").toString();
        if (!v.isObject() || QUuid(id).isNull() || ids.contains(id) ||
            !m.value("plan").canConvert<QVariantMap>() || (status != "open" && status != "reviewed") ||
            !QDateTime::fromString(m.value("date").toString(), Qt::ISODateWithMs).isValid()) {
            bad();
            return;
        }
        auto p = m.value("plan").toMap();
        auto checked = calculate(p.value("ticker").toString(), p.value("entry").toString(),
                                 p.value("budget").toString(), p.value("lotSize").toString(),
                                 p.value("feePercent").toString(), p.value("stop").toString(),
                                 p.value("target").toString(), p.value("riskBudget").toString());
        if (!checked.value("valid").toBool() || m.value("reason").toString().size() > 10000 ||
            m.value("exitReason").toString().size() > 10000) {
            bad();
            return;
        }
        m["plan"] = checked;
        m["ticker"] = checked["ticker"];
        ids.insert(id);
        loaded.append(m);
    }
    entries_ = loaded;
}
bool DecisionController::persist(const QVariantList &next) {
    if (!writable()) {
        error_ = "Запись недоступна: демонстрация, занятой профиль или повреждённый дневник";
        emit changed();
        return false;
    }
    QJsonObject obj{{"version", 1}, {"entries", QJsonArray::fromVariantList(next)}};
    auto bytes = QJsonDocument(obj).toJson();
    try {
        file_->write(bytes.toStdString());
    } catch (const std::exception &ex) {
        error_ = QString::fromUtf8(ex.what());
        emit changed();
        return false;
    }
    entries_ = next;
    error_.clear();
    emit changed();
    return true;
}
bool DecisionController::saveDecision(QVariantMap p, QString reason, QString exitReason) {
    auto checked =
        calculate(p.value("ticker").toString(), p.value("entry").toString(), p.value("budget").toString(),
                  p.value("lotSize").toString(), p.value("feePercent").toString(), p.value("stop").toString(),
                  p.value("target").toString(), p.value("riskBudget").toString());
    if (!checked.value("valid").toBool() || reason.trimmed().isEmpty() || reason.size() > 10000 ||
        exitReason.size() > 10000 || entries_.size() >= 10000) {
        error_ = "Проверьте план и заполните обоснование (до 10 000 символов)";
        emit changed();
        return false;
    }
    auto next = entries_;
    next.prepend(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                             {"date", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                             {"ticker", checked["ticker"]},
                             {"plan", checked},
                             {"reason", reason},
                             {"exitReason", exitReason},
                             {"status", "open"}});
    return persist(next);
}
bool DecisionController::updateDecision(QString id, QString reason, QString exitReason, QString status) {
    if (reason.trimmed().isEmpty() || reason.size() > 10000 || exitReason.size() > 10000 ||
        (status != "open" && status != "reviewed"))
        return false;
    auto next = entries_;
    for (auto &v : next) {
        auto m = v.toMap();
        if (m["id"].toString() == id) {
            m["reason"] = reason;
            m["exitReason"] = exitReason;
            m["status"] = status;
            v = m;
            return persist(next);
        }
    }
    return false;
}
bool DecisionController::deleteDecision(QString id) {
    auto next = entries_;
    for (int i = 0; i < next.size(); ++i)
        if (next[i].toMap()["id"].toString() == id) {
            next.removeAt(i);
            return persist(next);
        }
    return false;
}
