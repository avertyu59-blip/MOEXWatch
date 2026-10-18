#include "indicators.hpp"
#include "observation.hpp"
#include "portfolio_controller.hpp"
#include <QDateTime>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <map>

using namespace moexwatch;
namespace {
QString str(std::string_view value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}
QVariant value(std::optional<double> n) { return n && std::isfinite(*n) ? QVariant(*n) : QVariant{}; }
} // namespace
void PortfolioController::synchronizePositions() {
    const auto portfolio = calculate_portfolio(journal_.transactions, {});
    if (!portfolio)
        return;
    for (const auto &position : portfolio->positions)
        if (position.quantity > 0 &&
            std::none_of(instruments_.begin(), instruments_.end(),
                         [&](const auto &i) { return i.ticker == position.ticker; })) {
            Instrument item;
            item.ticker = position.ticker;
            item.quote.ticker = item.ticker;
            instruments_.push_back(std::move(item));
        }
}
void PortfolioController::saveViewSettings() {
    if (!settings_)
        return;
    settings_->setValue("overview/ticker", selected_ticker_);
    settings_->setValue("overview/sessions", chart_sessions_);
    settings_->setValue("overview/comparison", chart_comparison_);
    settings_->sync();
    if (settings_->status() != QSettings::NoError) {
        error_ = "Настройки обзора действуют только до закрытия: файл настроек "
                 "недоступен.";
        emit changed();
    }
}
void PortfolioController::setSelectedTicker(QString ticker) {
    ticker = ticker.trimmed().toUpper();
    if (ticker == selected_ticker_)
        return;
    if (std::none_of(instruments_.begin(), instruments_.end(),
                     [&](const auto &i) { return str(i.ticker) == ticker; }) && std::none_of(journal_.transactions.begin(),journal_.transactions.end(),[&](const auto& t){return str(t.ticker)==ticker;}))
        return;
    selected_ticker_ = std::move(ticker);
    rebuildOverview();
    saveViewSettings();
    emit selectionChanged();
    emit changed();
}
void PortfolioController::setChartSessions(int sessions) {
    if (sessions != 0 && sessions != 22 && sessions != 66 && sessions != 252)
        return;
    if (chart_sessions_ == sessions)
        return;
    chart_sessions_ = sessions;
    saveViewSettings();
    emit viewChanged();
}
void PortfolioController::setChartComparison(bool comparison) {
    if (chart_comparison_ == comparison)
        return;
    chart_comparison_ = comparison;
    saveViewSettings();
    emit viewChanged();
}
bool PortfolioController::addTicker(QString ticker) {
    ticker = ticker.trimmed().toUpper();
    if (!writable_) {
        fail("Наблюдения доступны только для чтения: профиль защищён или открыт "
             "режим демонстрации.");
        return false;
    }
    if (!valid_ticker(ticker.toStdString())) {
        fail("Тикер должен содержать допустимые символы, например SBER.");
        return false;
    }
    if (std::any_of(instruments_.begin(), instruments_.end(),
                    [&](const auto &i) { return str(i.ticker) == ticker; })) {
        setSelectedTicker(ticker);
        return true;
    }
    if (instruments_.size() >= 30) {
        fail("В списке наблюдения допускается не более 30 бумаг.");
        return false;
    }
    auto next = instruments_;
    Instrument item;
    item.ticker = ticker.toStdString();
    item.quote.ticker = item.ticker;
    next.push_back(std::move(item));
    try {
        storage_.save_watchlist(next);
    } catch (const std::exception &ex) {
        fail("Наблюдение не изменено: " + str(ex.what()), true);
        return false;
    }
    instruments_ = std::move(next);
    error_.clear();
    rebuild();
    setSelectedTicker(ticker);
    return true;
}
bool PortfolioController::removeTicker(QString ticker) {
    if (!writable_) {
        fail("Наблюдения доступны только для чтения.");
        return false;
    }
    const auto position = calculate_portfolio(journal_.transactions, {});
    if (position && std::any_of(position->positions.begin(), position->positions.end(),
                                [&](const auto &p) { return str(p.ticker) == ticker && p.quantity > 0; })) {
        fail("Бумага нужна для оценки открытой позиции. Закройте позицию в журнале "
             "перед удалением из наблюдения.");
        return false;
    }
    auto next = instruments_;
    const auto oldSize = next.size();
    std::erase_if(next, [&](const auto &i) { return str(i.ticker) == ticker; });
    if (next.size() == oldSize)
        return false;
    try {
        storage_.save_watchlist(next);
    } catch (const std::exception &ex) {
        fail("Наблюдение не изменено: " + str(ex.what()), true);
        return false;
    }
    instruments_ = std::move(next);
    error_.clear();
    rebuild();
    saveViewSettings();
    return true;
}
bool PortfolioController::setTarget(QString ticker, QString input) {
    if (!writable_) {
        fail("Цель доступна только для чтения.");
        return false;
    }
    std::optional<double> target;
    if (!input.trimmed().isEmpty()) {
        bool ok = false;
        const auto number = input.trimmed().replace(',', '.').toDouble(&ok);
        if (!ok || !std::isfinite(number) || number <= 0) {
            fail("Введите положительную целевую цену или оставьте поле пустым.");
            return false;
        }
        target = number;
    }
    auto next = instruments_;
    const auto i =
        std::find_if(next.begin(), next.end(), [&](const auto &item) { return str(item.ticker) == ticker; });
    if (i == next.end()) {
        fail("Бумага уже отсутствует в наблюдении.");
        return false;
    }
    i->target = target;
    try {
        storage_.save_watchlist(next);
    } catch (const std::exception &ex) {
        fail("Цель не сохранена: " + str(ex.what()), true);
        return false;
    }
    instruments_ = std::move(next);
    error_.clear();
    rebuildOverview();
    emit changed();
    return true;
}
void PortfolioController::rebuildOverview() {
    const auto previousSelection = selected_ticker_;
    if (std::none_of(instruments_.begin(), instruments_.end(),
                     [&](const auto &i) { return str(i.ticker) == selected_ticker_; })) {
        selected_ticker_ = instruments_.empty() ? QString{} : str(instruments_.front().ticker);
    }
    const auto now = QDateTime::currentSecsSinceEpoch();
    QList<QVariantMap> rows;
    for (const auto &i : instruments_) {
        const auto result = analyze(i.candles);
        const bool fresh = !i.quote_stale && fresh_moex_quote(i.quote, now);
        const auto state = i.quote.last ? (fresh ? QStringLiteral("Актуальная котировка")
                                                 : QStringLiteral("Котировка устарела"))
                                        : QStringLiteral("Котировка не загружена");
        rows.push_back({{"ticker", str(i.ticker)},
                        {"price", value(i.quote.last)},
                        {"change", value(i.quote.change_percent)},
                        {"rsi", value(result.rsi14)},
                        {"status", state},
                        {"stale", !fresh}});
    }
    if (rows != watchlist_.rows())
        watchlist_.replace(std::move(rows));
    QVariantMap info, levels{{"average", 0.0}, {"stop", 0.0}, {"take", 0.0}};
    QVariantList candles;
    const auto selected = std::find_if(instruments_.begin(), instruments_.end(),
                                       [&](const auto &i) { return str(i.ticker) == selected_ticker_; });
    if (selected != instruments_.end()) {
        const auto &i = *selected;
        const auto result = analyze(i.candles);
        const bool quoteFresh = !i.quote_stale && fresh_moex_quote(i.quote, now);
        info = {{"demo", demo_},
                {"ticker", str(i.ticker)},
                {"price", value(i.quote.last)},
                {"change", value(i.quote.change_percent)},
                {"rsi", value(result.rsi14)},
                {"sma20", value(result.sma20)},
                {"sma50", value(result.sma50)},
                {"sma200", value(result.sma200)},
                {"atr", value(result.atr14)},
                {"atrPercent", value(result.atr_percent)},
                {"volumeRatio", value(result.volume_ratio20)},
                {"drawdown", value(result.drawdown_percent)},
                {"target", value(i.target)},
                {"quoteStatus", quoteFresh     ? "Актуальная котировка"
                                : i.quote.last ? "Последняя котировка устарела"
                                               : "Котировка недоступна"},
                {"historyStatus", i.candles.empty() ? "История не загружена"
                                  : i.candles_stale ? "История из кэша / не подтверждена"
                                                    : "Дневная история обновлена"},
                {"historyTime", str(i.candles_checked_at)},
                {"sourceTime", str(i.quote.source_timestamp)},
                {"indexStatus",
                 index_stale_ ? "IMOEX: сохранённые / неполные данные" : "IMOEX: история обновлена"},
                {"indexError", index_error_},
                {"dataError", str(i.error)},
                {"candleCount", static_cast<int>(i.candles.size())}};
        const auto sma20 = sma_series(i.candles, 20), sma50 = sma_series(i.candles, 50),
                   sma200 = sma_series(i.candles, 200), rsi = rsi_series(i.candles);
        std::map<std::string, double> benchmark;
        for (const auto &c : index_candles_)
            benchmark[c.date.substr(0, 10)] = c.close;
        for (std::size_t n = 0; n < i.candles.size(); ++n) {
            const auto &c = i.candles[n];
            QVariantMap row{{"date", str(c.date.substr(0, 10))},
                            {"open", c.open},
                            {"high", c.high},
                            {"low", c.low},
                            {"close", c.close},
                            {"volume", c.volume},
                            {"sma20", value(sma20[n])},
                            {"sma50", value(sma50[n])},
                            {"sma200", value(sma200[n])},
                            {"rsi", value(rsi[n])}};
            if (const auto found = benchmark.find(c.date.substr(0, 10)); found != benchmark.end())
                row["benchmark"] = found->second;
            candles.push_back(row);
        }
        if (const auto portfolio = calculate_portfolio(journal_.transactions, {}))
            for (const auto &p : portfolio->positions)
                if (p.ticker == i.ticker && p.quantity > 0) {
                    levels["average"] = p.average_entry_price;
                    const auto level =
                        std::find_if(journal_.levels.begin(), journal_.levels.end(),
                                     [&](const auto &l) { return l.position_id == p.position_id; });
                    if (level != journal_.levels.end()) {
                        levels["stop"] = level->stop_loss.value_or(0);
                        levels["take"] = level->take_profit.value_or(0);
                    }
                    break;
                }
    }
    if (info != analysis_ || levels != chart_levels_ || candles != chart_candles_) {
        analysis_ = std::move(info);
        chart_levels_ = std::move(levels);
        chart_candles_ = std::move(candles);
        emit chartChanged();
    }
    if (previousSelection != selected_ticker_)
        emit selectionChanged();
}
