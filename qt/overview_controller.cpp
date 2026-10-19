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
    const auto portfolio = calculate_portfolio(portfolio_.journal().transactions, {});
    if (!portfolio)
        return;
    for (const auto &position : portfolio->positions)
        if (position.quantity > 0 &&
            std::none_of(market_.instruments().begin(), market_.instruments().end(),
                         [&](const auto &i) { return i.ticker == position.ticker; })) {
            Instrument item;
            item.ticker = position.ticker;
            item.quote.ticker = item.ticker;
            market_.instruments().push_back(std::move(item));
        }
}
void PortfolioController::saveViewSettings() {
    if (!workspace_.save()) {
        error_ = "Настройки обзора действуют только до закрытия: файл настроек недоступен.";
        emit statusChanged();
        emit changed();
    }
}

void PortfolioController::setSelectedTicker(QString ticker) {
    ticker = ticker.trimmed().toUpper();
    if (ticker == workspace_.state().ticker)
        return;
    if (std::none_of(market_.instruments().begin(), market_.instruments().end(),
                     [&](const auto &i) { return str(i.ticker) == ticker; }) &&
        std::none_of(portfolio_.journal().transactions.begin(), portfolio_.journal().transactions.end(),
                     [&](const auto &t) { return str(t.ticker) == ticker; }))
        return;
    workspace_.state().ticker = std::move(ticker);
    rebuildOverview();
    saveViewSettings();
    emit selectionChanged();
    emit statusChanged();
    emit changed();
}
void PortfolioController::setChartSessions(int sessions) {
    if (sessions != 0 && sessions != 22 && sessions != 66 && sessions != 252)
        return;
    if (workspace_.state().sessions == sessions)
        return;
    workspace_.state().sessions = sessions;
    saveViewSettings();
    emit viewChanged();
}
void PortfolioController::setChartComparison(bool comparison) {
    if (workspace_.state().comparison == comparison)
        return;
    workspace_.state().comparison = comparison;
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
    if (std::any_of(market_.instruments().begin(), market_.instruments().end(),
                    [&](const auto &i) { return str(i.ticker) == ticker; })) {
        setSelectedTicker(ticker);
        return true;
    }
    if (market_.instruments().size() >= 30) {
        fail("В списке наблюдения допускается не более 30 бумаг.");
        return false;
    }
    auto next = market_.instruments();
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
    market_.replace(std::move(next));
    error_.clear();
    rebuild();
    setSelectedTicker(ticker);
    emit marketDataChanged();
    return true;
}
bool PortfolioController::removeTicker(QString ticker) {
    if (!writable_) {
        fail("Наблюдения доступны только для чтения.");
        return false;
    }
    const auto position = calculate_portfolio(portfolio_.journal().transactions, {});
    if (position && std::any_of(position->positions.begin(), position->positions.end(),
                                [&](const auto &p) { return str(p.ticker) == ticker && p.quantity > 0; })) {
        fail("Бумага нужна для оценки открытой позиции. Закройте позицию в журнале "
             "перед удалением из наблюдения.");
        return false;
    }
    auto next = market_.instruments();
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
    market_.replace(std::move(next));
    error_.clear();
    rebuild();
    saveViewSettings();
    emit marketDataChanged();
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
    auto next = market_.instruments();
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
    market_.replace(std::move(next));
    error_.clear();
    rebuildOverview();
    emit marketDataChanged();
    emit statusChanged();
    emit changed();
    return true;
}
void PortfolioController::rebuildOverview() {
    const auto previousSelection = workspace_.state().ticker;
    if (std::none_of(market_.instruments().begin(), market_.instruments().end(),
                     [&](const auto &i) { return str(i.ticker) == workspace_.state().ticker; }) &&
        std::any_of(
            portfolio_.journal().transactions.begin(), portfolio_.journal().transactions.end(),
            [&](const auto &operation) { return str(operation.ticker) == workspace_.state().ticker; })) {
        Instrument instrument;
        instrument.ticker = workspace_.state().ticker.toStdString();
        instrument.quote.ticker = instrument.ticker;
        market_.instruments().push_back(std::move(instrument));
    }
    if (std::none_of(market_.instruments().begin(), market_.instruments().end(),
                     [&](const auto &i) { return str(i.ticker) == workspace_.state().ticker; })) {
        workspace_.state().ticker =
            market_.instruments().empty() ? QString{} : str(market_.instruments().front().ticker);
    }
    const auto now = QDateTime::currentSecsSinceEpoch();
    QList<QVariantMap> rows;
    for (const auto &i : market_.instruments()) {
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
    const auto selected =
        std::find_if(market_.instruments().begin(), market_.instruments().end(),
                     [&](const auto &i) { return str(i.ticker) == workspace_.state().ticker; });
    if (selected != market_.instruments().end()) {
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
                 market_.indexStale() ? "IMOEX: сохранённые / неполные данные" : "IMOEX: история обновлена"},
                {"indexError", str(market_.indexError())},
                {"dataError", str(i.error)},
                {"candleCount", static_cast<int>(i.candles.size())}};
        const auto sma20 = sma_series(i.candles, 20), sma50 = sma_series(i.candles, 50),
                   sma200 = sma_series(i.candles, 200), rsi = rsi_series(i.candles);
        std::map<std::string, double> benchmark;
        for (const auto &c : market_.index())
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
        if (const auto portfolio = calculate_portfolio(portfolio_.journal().transactions, {}))
            for (const auto &p : portfolio->positions)
                if (p.ticker == i.ticker && p.quantity > 0) {
                    levels["average"] = p.average_entry_price;
                    const auto level =
                        std::find_if(portfolio_.journal().levels.begin(), portfolio_.journal().levels.end(),
                                     [&](const auto &l) { return l.position_id == p.position_id; });
                    if (level != portfolio_.journal().levels.end()) {
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
    if (previousSelection != workspace_.state().ticker)
        emit selectionChanged();
}
