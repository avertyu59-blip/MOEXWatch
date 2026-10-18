#include "signals_controller.hpp"
#include "signal_rules.hpp"
#include <QDateTime>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <limits>
using namespace moexwatch;
namespace {
QVariant num(std::optional<double> v) { return v && std::isfinite(*v) ? QVariant(*v) : QVariant(); }
QString reasons(const SignalEvaluation &e) {
    QStringList r;
    for (const auto &s : e.reasons)
        r << QString::fromStdString(s);
    return r.join("; ");
}
std::optional<double> price(QString v) {
    bool ok = false;
    v = v.trimmed();
    v.replace(',', '.');
    double n = v.toDouble(&ok);
    return ok && std::isfinite(n) && n > 0 ? std::optional(n) : std::nullopt;
}
} // namespace
SignalsController::SignalsController(PortfolioController *market, QObject *parent)
    : QObject(parent), market_(market), storage_(market->dataRoot()) {
    try {
        settings_ = storage_.load_signal_settings({}, market_->writable() && !market_->demo());
    } catch (const std::exception &e) {
        settings_ok_ = false;
        error_ = QString::fromUtf8(e.what());
    }
    try {
        if (auto s = storage_.read_text("price_alerts.csv"))
            observer_.configure(parse_price_alerts(*s));
        if (auto s = storage_.read_text("signal_history.csv"))
            events_ = parse_signal_history(*s);
    } catch (const std::exception &e) {
        files_ok_ = false;
        error_ += QStringLiteral("\n") + QString::fromUtf8(e.what()) +
                  QStringLiteral(". Исходный файл сохранён; запись наблюдений заблокирована.");
    }
    connect(market_, &PortfolioController::changed, this, &SignalsController::rebuild);
    connect(market_, &PortfolioController::chartChanged, this, &SignalsController::rebuild);
    connect(&timer_, &QTimer::timeout, this, [this] {
        if (!market_->busy() && !market_->demo())
            market_->refresh();
    });
    timerSettings();
    rebuild();
}
bool SignalsController::writable() const { return market_->writable() && !market_->demo() && files_ok_; }
QString SignalsController::summary() const {
    return QStringLiteral("Совпадений: %1 · Бумаг скринера: %2 · История: %3. Проверка работает, пока "
                          "приложение открыто.")
        .arg(matches_.rowCount())
        .arg(screener_.rowCount())
        .arg(events_.size());
}
QVariantMap SignalsController::settings() const {
    return {{"oversold", settings_.rsi_oversold_enabled},
            {"oversoldBelow", settings_.rsi_oversold_below},
            {"overbought", settings_.rsi_overbought_enabled},
            {"overboughtAbove", settings_.rsi_overbought_above},
            {"aboveSma", settings_.above_sma200_enabled},
            {"nearTarget", settings_.near_target_enabled},
            {"targetPercent", settings_.near_target_percent},
            {"all", settings_.require_all},
            {"notifications", settings_.notifications_enabled},
            {"autoRefresh", settings_.auto_refresh_enabled},
            {"minutes", settings_.auto_refresh_minutes}};
}
void SignalsController::timerSettings() {
    timer_.stop();
    if (settings_.auto_refresh_enabled && !market_->demo())
        timer_.start(settings_.auto_refresh_minutes * 60000);
}
bool SignalsController::saveSettings(QVariantMap m) {
    if (!writable() || !settings_ok_)
        return false;
    for (const char *key : {"oversold", "oversoldBelow", "overbought", "overboughtAbove", "aboveSma",
                            "nearTarget", "targetPercent", "all", "notifications", "autoRefresh", "minutes"})
        if (!m.contains(QLatin1String(key))) {
            error_ = QStringLiteral("Неполные настройки сигналов.");
            emit changed();
            return false;
        }
    for (const char *key : {"oversoldBelow", "overboughtAbove", "targetPercent", "minutes"}) {
        bool ok = false;
        const double value = m.value(QLatin1String(key)).toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            error_ = QStringLiteral("Настройки должны содержать конечные числа.");
            emit changed();
            return false;
        }
    }
    auto s = settings_;
    s.rsi_oversold_enabled = m.value("oversold").toBool();
    s.rsi_oversold_below = m.value("oversoldBelow").toDouble();
    s.rsi_overbought_enabled = m.value("overbought").toBool();
    s.rsi_overbought_above = m.value("overboughtAbove").toDouble();
    s.above_sma200_enabled = m.value("aboveSma").toBool();
    s.near_target_enabled = m.value("nearTarget").toBool();
    s.near_target_percent = m.value("targetPercent").toDouble();
    s.require_all = m.value("all").toBool();
    s.notifications_enabled = m.value("notifications").toBool();
    s.auto_refresh_enabled = m.value("autoRefresh").toBool();
    s.auto_refresh_minutes = m.value("minutes").toInt();
    if (!valid_signal_settings(s)) {
        error_ = QStringLiteral("Пороги: 0–100; интервал: 15, 30 или 60 минут.");
        emit changed();
        return false;
    }
    try {
        storage_.save_signal_settings(s);
    } catch (const std::exception &e) {
        error_ = QString::fromUtf8(e.what());
        emit changed();
        return false;
    }
    if (!same_technical_rules(settings_, s))
        observer_.reset_technical();
    settings_ = s;
    error_.clear();
    timerSettings();
    rebuild();
    return true;
}
bool SignalsController::setFilters(QVariantMap f) {
    bool ok1 = false, ok2 = false;
    double volume = f.value("volumeMin", 0).toDouble(&ok1), atr = f.value("atrMin", 0).toDouble(&ok2);
    int kind = f.value("kind").toInt();
    if (!ok1 || !ok2 || !std::isfinite(volume) || !std::isfinite(atr) || volume < 0 || atr < 0 || kind < 0 ||
        kind > 4) {
        error_ = QStringLiteral("Фильтры объёма и ATR должны быть неотрицательными конечными числами.");
        emit changed();
        return false;
    }
    filters_ = {{"kind", kind}, {"volumeMin", volume}, {"atrMin", atr}};
    rebuild();
    return true;
}
void SignalsController::sort(QString key, bool asc) {
    if (!QStringList{"ticker", "price", "change", "rsi", "atr", "volume"}.contains(key))
        return;
    sort_ = key;
    ascending_ = asc;
    rebuild();
}
bool SignalsController::persistAlerts(std::vector<PriceAlert> rules) {
    if (!writable())
        return false;
    try {
        const auto bytes = serialize_price_alerts(rules);
        storage_.atomic_write("price_alerts.csv", bytes);
        observer_.configure(std::move(rules));
        error_.clear();
        rebuild();
        return true;
    } catch (const std::exception &e) {
        error_ = QString::fromUtf8(e.what());
        emit changed();
        return false;
    }
}
bool SignalsController::addAlert(QString ticker, bool below, QString p) {
    ticker = ticker.trimmed().toUpper();
    auto level = price(p);
    if (!valid_ticker(ticker.toStdString()) || !level) {
        error_ = QStringLiteral("Укажите тикер и положительную конечную цену.");
        emit changed();
        return false;
    }
    if (!market_->tickers().contains(ticker)) {
        error_ = QStringLiteral("Сначала добавьте бумагу в список наблюдения.");
        emit changed();
        return false;
    }
    auto rules = observer_.rules();
    std::uint64_t id = 1;
    for (const auto &r : rules) {
        if (r.ticker == ticker.toStdString() && r.at_or_below == below && r.level == *level) {
            error_ = QStringLiteral("Такое ценовое условие уже существует.");
            emit changed();
            return false;
        }
        if (r.id == std::numeric_limits<std::uint64_t>::max())
            return false;
        id = std::max(id, r.id + 1);
    }
    rules.push_back({id, ticker.toStdString(), below, *level, true});
    return persistAlerts(std::move(rules));
}
bool SignalsController::deleteAlert(QString id) {
    auto rules = observer_.rules();
    auto n = rules.size();
    std::erase_if(rules, [&](const auto &r) { return QString::number(r.id) == id; });
    return n != rules.size() && persistAlerts(std::move(rules));
}
bool SignalsController::setAlertEnabled(QString id, bool enabled) {
    auto rules = observer_.rules();
    for (auto &r : rules)
        if (QString::number(r.id) == id) {
            r.enabled = enabled;
            return persistAlerts(std::move(rules));
        }
    return false;
}
bool SignalsController::clearHistory() {
    if (!writable())
        return false;
    try {
        storage_.atomic_write("signal_history.csv", serialize_signal_history({}));
        events_.clear();
        refreshModels();
        emit changed();
        return true;
    } catch (const std::exception &e) {
        error_ = QString::fromUtf8(e.what());
        emit changed();
        return false;
    }
}
void SignalsController::openTicker(QString ticker) {
    if (!market_->tickers().contains(ticker))
        return;
    market_->setSelectedTicker(ticker);
    emit openChart();
}
void SignalsController::rebuild() {
    if (rebuilding_)
        return;
    rebuilding_ = true;
    QList<QVariantMap> rows, matched;
    auto now = QDateTime::currentSecsSinceEpoch();
    bool eventsAdded = false;
    for (const auto &i : market_->instrumentsSnapshot()) {
        const auto a = analyze(i.candles);
        const bool fresh = !i.quote_stale && fresh_moex_quote(i.quote, now);
        const auto e = evaluate_signals(a, i.quote.last, i.target, !fresh, i.candles_stale, settings_);
        QVariantMap row{{"ticker", QString::fromStdString(i.ticker)},
                        {"price", num(i.quote.last)},
                        {"change", num(i.quote.change_percent)},
                        {"rsi", num(a.rsi14)},
                        {"atr", num(a.atr_percent)},
                        {"volume", num(a.volume_ratio20)},
                        {"reasons", reasons(e)},
                        {"stale", !fresh || i.candles_stale}};
        if (e.matched)
            matched.append(row);
        const int kind = filters_["kind"].toInt();
        const double vol = filters_["volumeMin"].toDouble(), atr = filters_["atrMin"].toDouble();
        bool pass = kind == 0 || matches_filter(a, i.quote.last, i.target, !fresh || i.candles_stale,
                                                static_cast<WatchFilter>(kind));
        if (vol > 0)
            pass = pass && fresh && !i.candles_stale && a.volume_ratio20 && *a.volume_ratio20 >= vol;
        if (atr > 0)
            pass = pass && fresh && !i.candles_stale && a.atr_percent && *a.atr_percent >= atr;
        if (pass) {
            QStringList why;
            if (kind == 1)
                why << QStringLiteral("RSI < 30");
            if (kind == 2)
                why << QStringLiteral("RSI > 70");
            if (kind == 3)
                why << QStringLiteral("Цена выше SMA200");
            if (kind == 4)
                why << QStringLiteral("До цели ≤ 5%");
            if (vol > 0)
                why << QStringLiteral("Объём ≥ %1×").arg(vol);
            if (atr > 0)
                why << QStringLiteral("ATR ≥ %1%").arg(atr);
            if (why.isEmpty())
                why << QStringLiteral("Все бумаги наблюдения");
            row["reasons"] = why.join("; ");
            rows.append(row);
        }
        if (!market_->demo())
            for (auto &event :
                 observer_.observe(i.quote, fresh, !i.candles_stale, e,
                                   settings_.require_all ? "Все условия" : "Любое условие", now)) {
                events_.push_back(event);
                eventsAdded = true;
                if (settings_.notifications_enabled)
                    emit notification(QStringLiteral("Новый сигнал · ") + QString::fromStdString(i.ticker),
                                      QString::fromStdString(event.condition));
            }
    }
    const auto less = [this](const QVariantMap &a, const QVariantMap &b) {
        if (sort_ == "ticker")
            return ascending_ ? a["ticker"].toString() < b["ticker"].toString()
                              : a["ticker"].toString() > b["ticker"].toString();
        const auto optional = [](QVariant n) -> std::optional<double> {
            return n.isValid() && !n.isNull() ? std::optional(n.toDouble()) : std::nullopt;
        };
        auto x = optional(a[sort_]), y = optional(b[sort_]);
        if (x == y)
            return a["ticker"].toString() < b["ticker"].toString();
        return optional_number_less(x, y, ascending_);
    };
    std::stable_sort(rows.begin(), rows.end(), less);
    std::stable_sort(matched.begin(), matched.end(), less);
    screener_.replace(rows);
    matches_.replace(matched);
    if (events_.size() > 2000)
        events_.erase(events_.begin(), events_.end() - 2000);
    if (eventsAdded && writable())
        try {
            storage_.atomic_write("signal_history.csv", serialize_signal_history(events_));
        } catch (const std::exception &e) {
            error_ = QString::fromUtf8(e.what());
        }
    refreshModels();
    rebuilding_ = false;
    emit changed();
}
void SignalsController::refreshModels() {
    QList<QVariantMap> alerts, history;
    for (const auto &r : observer_.rules())
        alerts.append({{"id", QString::number(r.id)},
                       {"ticker", QString::fromStdString(r.ticker)},
                       {"below", r.at_or_below},
                       {"level", r.level},
                       {"enabled", r.enabled}});
    for (auto it = events_.rbegin(); it != events_.rend(); ++it)
        history.append({{"date", QString::fromStdString(it->detected_at)},
                        {"ticker", QString::fromStdString(it->ticker)},
                        {"kind", QString::fromStdString(it->kind)},
                        {"condition", QString::fromStdString(it->condition)},
                        {"price", it->price},
                        {"quoteTime", QString::fromStdString(it->quote_time)}});
    alerts_.replace(alerts);
    history_.replace(history);
}
