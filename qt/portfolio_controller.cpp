#include "portfolio_controller.hpp"
#include "indicators.hpp"
#include "market_data.hpp"
#include "observation.hpp"
#include "portfolio_overview.hpp"
#include "storage.hpp"
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QUrl>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <unordered_map>

using namespace moexwatch;
namespace {
QString text(std::string_view s) { return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())); }
QString number(double n, int digits = 2) { return QLocale(QLocale::Russian).toString(n, 'f', digits); }
QString optional(std::optional<double> n, QString suffix = {}) {
    return n ? number(*n) + suffix : QStringLiteral("—");
}
std::optional<double> parse(QString value) {
    auto bytes = value.trimmed().replace(',', '.').toStdString();
    double result{};
    const auto [end, error] = std::from_chars(bytes.data(), bytes.data() + bytes.size(), result);
    if (error != std::errc{} || end != bytes.data() + bytes.size() || !std::isfinite(result))
        return {};
    return result;
}
QString pathText(const std::filesystem::path &path) {
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}
} // namespace

PortfolioController::PortfolioController(std::filesystem::path root, Fetch fetch, QObject *parent)
    : QObject(parent), storage_(std::move(root)), portfolio_(storage_), fetch_(std::move(fetch)) {
    try {
        const auto dir = pathText(storage_.root());
        if (!QDir().mkpath(dir))
            throw std::runtime_error("Не удалось создать каталог данных");
        profile_lock_ = std::make_unique<ProfileLock>(storage_.root());
        if (!profile_lock_->ownsLock())
            throw std::runtime_error("Другая копия MOEXWatch уже использует этот профиль");
        lock_ = std::make_unique<QLockFile>(dir + "/qt-session.lock");
        if (!lock_->tryLock(0))
            throw std::runtime_error("Другая Qt-копия уже использует этот профиль");
        // Settings failure must never disable financial journal writes.
        workspace_.open(dir + "/qt-workspace.ini");
        portfolio_.load();
        market_.load(storage_);
        synchronizePositions();
        market_.loadCaches(storage_);
        status_ = "Данные загружены. Обновите котировки для оценки позиций.";
    } catch (const std::exception &ex) {
        fail("Не удалось загрузить профиль: " + text(ex.what()), true);
    }
    if (!fetch_) {
        std::filesystem::path cache;
        try {
            cache = storage_.root();
        } catch (...) { /* Startup error already reported; keep UI available. */
        }
        fetch_ = MarketDataService::source(cache);
    }

    loadCash();
    connect(&timer_, &QTimer::timeout, this, &PortfolioController::drain);
    timer_.start(50);
    connect(&freshness_timer_, &QTimer::timeout, this, [this] {
        rebuild();
        emit marketDataChanged();
    });
    freshness_timer_.start(60000);
    rebuild();
}
PortfolioController::~PortfolioController() { shutdown(); }
void PortfolioController::shutdown() {
    if (closed_)
        return;
    closed_ = true;
    timer_.stop();
    freshness_timer_.stop();
    mailbox_->close();
    tasks_.shutdown();
    active_request_ = 0;
}
void PortfolioController::fail(QString message, bool blockWrites) {
    error_ = std::move(message);
    if (blockWrites && writable_) {
        writable_ = false;
        emit writableChanged();
    }
    emit statusChanged();
    emit changed();
}
void PortfolioController::clearError() {
    error_.clear();
    emit statusChanged();
    emit changed();
}
void PortfolioController::setDark(bool value) {
    if (workspace_.state().dark == value)
        return;
    workspace_.state().dark = value;
    if (!workspace_.save())
        error_ = "Тема изменена только для этой сессии: настройки недоступны для записи.";
    emit settingsChanged();
    emit statusChanged();
    emit changed();
}
QStringList PortfolioController::tickers() const {
    QStringList values;
    for (const auto &i : market_.instruments())
        values.push_back(text(i.ticker));
    values.removeDuplicates();
    values.sort();
    return values;
}
bool PortfolioController::persist(PortfolioJournal candidate) {
    if (!writable_ || closed_ || demo_) {
        fail("Запись заблокирована. Сначала восстановите профиль из резервной копии.");
        return false;
    }
    if (!valid_portfolio_journal(candidate)) {
        fail("Изменение нарушает связи операций и уровней позиции; журнал не изменён.");
        return false;
    }
    try {
        const auto before = tickers();
        portfolio_.commit(std::move(candidate));
        error_.clear();
        rebuild();
        emit journalChanged();
        if (tickers() != before)
            emit marketDataChanged();
        return true;
    } catch (const std::exception &ex) {
        fail("Журнал не изменён: " + text(ex.what()), true);
        return false;
    }
}
QVariantMap PortfolioController::incomeWithDividends(double received, bool valid) const {
    const auto income = valid && totals_.value("calculationValid").toBool()
                            ? calculate_portfolio_income(
                                  totals_.value("realizedValue").toDouble(),
                                  totals_.value("complete").toBool()
                                      ? std::optional<double>(totals_.value("unrealizedValue").toDouble())
                                      : std::optional<double>{},
                                  received)
                            : std::optional<PortfolioIncome>{};
    return {{"realized", income ? number(income->realized_with_dividends) + " ₽" : QStringLiteral("—")},
            {"total", income ? optional(income->total_profit, " ₽") : QStringLiteral("—")}};
}
void PortfolioController::loadCash() {
    if (demo_)
        return;
    try {
        cash_file_ = std::make_unique<GuardedFile>(storage_.root() / "portfolio_cash_v1.json", 65536);
        const auto bytes = cash_file_->read();
        if (!bytes)
            return;
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(*bytes), &error);
        const auto obj = doc.object();
        if (error.error != QJsonParseError::NoError || !doc.isObject() || !obj.value("version").isDouble() ||
            obj.value("version").toDouble() != 1 || !obj.value("cash").isDouble() ||
            !std::isfinite(obj.value("cash").toDouble()) || obj.value("cash").toDouble() < 0 ||
            obj.value("cash").toDouble() > 1e15)
            throw std::runtime_error(
                "Файл свободных средств повреждён или имеет неизвестную версию; запись суммы заблокирована.");
        free_cash_ = obj.value("cash").toDouble();
    } catch (const std::exception &ex) {
        cash_valid_ = false;
        cash_error_ = text(ex.what());
    }
}
bool PortfolioController::setFreeCash(QString amount) {
    if (!cashWritable()) {
        fail("Сохранение свободных средств недоступно. " + cash_error_);
        return false;
    }
    const auto value = parse(amount);
    if (!value || *value < 0 || *value > 1e15) {
        fail("Введите неотрицательную конечную сумму свободных средств (не более 10^15 ₽).");
        return false;
    }
    try {
        const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"cash", *value}}).toJson();
        cash_file_->write(bytes.toStdString());
        free_cash_ = *value;
        cash_error_.clear();
        error_.clear();
        rebuild();
        return true;
    } catch (const std::exception &ex) {
        cash_valid_ = false;
        cash_error_ = text(ex.what());
        fail("Свободные средства не изменены: " + cash_error_);
        rebuild();
        return false;
    }
}
bool PortfolioController::commitImportedTransactions(std::vector<PortfolioTransaction> transactions,
                                                     quint64 expectedRevision) {
    if (!writable_ || closed_ || demo_) {
        fail("Запись недоступна");
        return false;
    }
    try {
        const auto before = tickers();
        portfolio_.import_transactions(std::move(transactions), expectedRevision);
        error_.clear();
        rebuild();
        emit journalChanged();
        if (tickers() != before)
            emit marketDataChanged();
        return true;
    } catch (const std::exception &ex) {
        fail(text(ex.what()));
        return false;
    }
}

bool PortfolioController::addOperation(QString date, QString ticker, int side, QString quantity,
                                       QString price, QString fee) {
    ticker = ticker.trimmed().toUpper();
    date = date.trimmed();
    const auto q = parse(quantity), p = parse(price),
               f = fee.trimmed().isEmpty() ? std::optional<double>{0} : parse(fee);
    QStringList invalid;
    if (QDate::fromString(date, Qt::ISODate).toString(Qt::ISODate) != date)
        invalid.push_back("дату в формате ГГГГ-ММ-ДД");
    if (side < 0 || side > 1)
        invalid.push_back("сторону операции");
    if (!q)
        invalid.push_back("количество акций");
    if (!p)
        invalid.push_back("цену за акцию");
    if (!f)
        invalid.push_back("комиссию");
    if (!invalid.isEmpty()) {
        fail("Проверьте: " + invalid.join(", ") + ".");
        return false;
    }
    PortfolioTransaction operation{date.toStdString(),
                                   ticker.toStdString(),
                                   side == 0 ? PortfolioSide::Buy : PortfolioSide::Sell,
                                   *q,
                                   *p,
                                   *f};
    if (!valid_portfolio_transaction(operation)) {
        fail("Тикер некорректен, количество/цена не положительны или комиссия отрицательна.");
        return false;
    }
    try {
        return persist(portfolio_.with_operation(std::move(operation)));
    } catch (const std::exception &ex) {
        fail(text(ex.what()));
        return false;
    }
}
bool PortfolioController::deleteOperation(QString id) {
    try {
        return persist(portfolio_.without_operation(id.toStdString()));
    } catch (const std::exception &ex) {
        fail("Удаление нарушает историю позиции: " + text(ex.what()));
        return false;
    }
}

QVariantMap PortfolioController::position(QString id) const {
    for (const auto &row : positions_.rows())
        if (row.value("id").toString() == id)
            return row;
    return {};
}
bool PortfolioController::saveLevels(QString id, QString stop, QString take) {
    const auto row = position(id);
    if (row.isEmpty()) {
        fail("Открытая позиция уже отсутствует.");
        return false;
    }
    const auto s = stop.trimmed().isEmpty() ? std::optional<double>{} : parse(stop);
    const auto t = take.trimmed().isEmpty() ? std::optional<double>{} : parse(take);
    if ((!stop.trimmed().isEmpty() && !s) || (!take.trimmed().isEmpty() && !t)) {
        fail("Введите положительные уровни или оставьте поля пустыми.");
        return false;
    }
    PositionLevels level{row.value("ticker").toString().toStdString(),
                         row.value("opened").toString().toStdString(), s, t, id.toStdString()};
    if (!valid_position_levels(level)) {
        fail("Проверьте стоп-лосс и тейк-профит.");
        return false;
    }
    auto next = portfolio_.journal();
    std::erase_if(next.levels, [&](const auto &l) { return l.position_id == level.position_id; });
    next.levels.push_back(level);
    return persist(std::move(next));
}
void PortfolioController::sortPositions(QString key) {
    if (key != "ticker" && key != "weight" && key != "profit")
        return;
    sort_key_ = std::move(key);
    rebuild();
    emit viewChanged();
}
void PortfolioController::rebuild() {
    const auto previousTotals = totals_;
    const auto previousAllocation = allocation_;
    synchronizePositions();
    std::vector<Quote> quotes;
    for (const auto &i : market_.instruments())
        quotes.push_back(i.quote);
    const auto result = portfolio_.calculate(quotes);
    QList<QVariantMap> rows, operations;
    allocation_.clear();
    const auto now = QDateTime::currentSecsSinceEpoch();
    bool fresh = static_cast<bool>(result);
    if (result)
        for (const auto &p : result->positions)
            if (p.quantity > 0) {
                const auto i = std::find_if(market_.instruments().begin(), market_.instruments().end(),
                                            [&](const auto &item) { return item.ticker == p.ticker; });
                if (i == market_.instruments().end() || i->quote_stale || !fresh_moex_quote(i->quote, now) ||
                    !p.market_value)
                    fresh = false;
            }
    const bool complete = result && result->open_positions == result->priced_positions;
    if (result)
        for (const auto &p : result->positions)
            if (p.quantity > 0) {
                const auto i = std::find_if(market_.instruments().begin(), market_.instruments().end(),
                                            [&](const auto &item) { return item.ticker == p.ticker; });
                const bool stale =
                    i == market_.instruments().end() || i->quote_stale || !fresh_moex_quote(i->quote, now);
                auto level =
                    std::find_if(portfolio_.journal().levels.begin(), portfolio_.journal().levels.end(),
                                 [&](const auto &l) { return l.position_id == p.position_id; });
                const auto weight = portfolio_weight(*result, p, fresh);
                rows.push_back(
                    {{"id", text(p.position_id)},
                     {"ticker", text(p.ticker)},
                     {"opened", text(p.opened_on)},
                     {"quantity", number(p.quantity, p.quantity == std::floor(p.quantity) ? 0 : 4)},
                     {"average", number(p.average_entry_price)},
                     {"price", optional(p.current_price)},
                     {"value", optional(p.market_value)},
                     {"profit", optional(p.unrealized_profit)},
                     {"percent", optional(p.unrealized_percent, "%")},
                     {"profitValue", p.unrealized_profit.value_or(0)},
                     {"weightValue", weight.value_or(0)},
                     {"weight", optional(weight, "%")},
                     {"stale", stale},
                     {"missing", !p.current_price.has_value()},
                     {"stop", level != portfolio_.journal().levels.end() && level->stop_loss
                                  ? QString::number(*level->stop_loss, 'g', 15)
                                  : QString{}},
                     {"take", level != portfolio_.journal().levels.end() && level->take_profit
                                  ? QString::number(*level->take_profit, 'g', 15)
                                  : QString{}}});
            }
    std::stable_sort(rows.begin(), rows.end(), [&](const auto &a, const auto &b) {
        if (sort_key_ == "ticker")
            return a.value("ticker").toString() < b.value("ticker").toString();
        const auto key = sort_key_ == "weight" ? "weightValue" : "profitValue";
        return a.value(key).toDouble() > b.value(key).toDouble();
    });
    if (fresh && result && result->total_market_value > 0)
        for (const auto &row : rows)
            allocation_.push_back(QVariantMap{{"ticker", row.value("ticker")},
                                              {"weight", row.value("weightValue")},
                                              {"value", row.value("value")}});
    positions_.replace(std::move(rows));
    auto ordered = portfolio_.journal().transactions;
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
        return a.date != b.date ? a.date > b.date : a.sequence > b.sequence;
    });
    for (const auto &t : ordered)
        operations.push_back({{"id", text(t.id)},
                              {"positionId", text(t.position_id)},
                              {"date", text(t.date)},
                              {"ticker", text(t.ticker)},
                              {"side", t.side == PortfolioSide::Buy ? "Покупка" : "Продажа"},
                              {"quantity", number(t.quantity, 4)},
                              {"price", number(t.price)},
                              {"fee", number(t.commission)}});
    operations_.replace(std::move(operations));
    totals_ = {{"value", result && complete ? number(result->total_market_value) + " ₽" : "—"},
               {"unrealized", result && complete ? number(result->total_unrealized_profit) + " ₽" : "—"},
               {"realized", result ? number(result->total_realized_profit) + " ₽" : "—"},
               {"unrealizedValue", result ? result->total_unrealized_profit : 0},
               {"realizedValue", result ? result->total_realized_profit : 0},
               {"fresh", fresh},
               {"complete", complete},
               {"openCount", result ? static_cast<int>(result->open_positions) : 0},
               {"valuationNote", fresh      ? "По актуальным котировкам"
                                 : complete ? "Последняя оценка · котировки устарели"
                                            : "Нет котировок всех позиций"}};
    std::vector<PositionQuoteQuality> quality;
    if (result)
        for (const auto &position : result->positions)
            if (position.quantity > 0) {
                const auto found =
                    std::find_if(market_.instruments().begin(), market_.instruments().end(),
                                 [&](const auto &item) { return item.ticker == position.ticker; });
                auto state = QuoteQuality::Missing;
                if (position.market_value && found != market_.instruments().end())
                    state = found->quote_stale || !fresh_moex_quote(found->quote, now) ? QuoteQuality::Stale
                                                                                       : QuoteQuality::Fresh;
                quality.push_back({position.ticker, state});
            }
    const auto overview = result
                              ? calculate_portfolio_overview(*result, quality, cash_valid_ ? free_cash_ : 0)
                              : std::optional<PortfolioOverview>{};
    totals_["calculationValid"] = result.has_value();
    totals_["cost"] = overview ? number(overview->remaining_cost) + " ₽" : "—";
    totals_["cash"] = cash_valid_ ? number(free_cash_) + " ₽" : "—";
    totals_["cashRaw"] = cash_valid_ ? QString::number(free_cash_, 'g', 15) : QString{};
    totals_["cashValid"] = cash_valid_;
    totals_["cashError"] = cash_error_;
    totals_["totalAssets"] =
        overview && cash_valid_ ? optional(overview->total_assets, " ₽") : QStringLiteral("—");
    totals_["freshCount"] = overview ? static_cast<int>(overview->fresh_count) : 0;
    totals_["staleCount"] = overview ? static_cast<int>(overview->stale_count) : 0;
    totals_["missingCount"] = overview ? static_cast<int>(overview->missing_count) : 0;
    totals_["top3"] = overview ? optional(overview->top3_percent, " %") : QStringLiteral("—");
    rebuildOverview();
    if (totals_ != previousTotals || allocation_ != previousAllocation)
        emit portfolioChanged();
    emit statusChanged();
    emit changed();
}
void PortfolioController::refresh() {
    if (demo_) {
        status_ = "Демо работает без сети и не изменяет реальный профиль.";
        emit statusChanged();
        emit changed();
        return;
    }
    if (closed_ || busy() || tasks_.running(TaskKind::Market))
        return;
    auto input = market_.instruments();
    for (const auto &ticker : tickers())
        if (std::none_of(input.begin(), input.end(),
                         [&](const auto &i) { return text(i.ticker) == ticker; })) {
            Instrument i;
            i.ticker = ticker.toStdString();
            i.quote.ticker = i.ticker;
            input.push_back(std::move(i));
        }
    if (input.empty()) {
        status_ = "Сначала добавьте тикер на странице «Обзор» или запишите операцию.";
        emit statusChanged();
        emit changed();
        return;
    }
    const auto request = ++request_sequence_;
    active_request_ = request;
    const auto fetch = fetch_;
    status_ = "Обновление котировок MOEX…";
    try {
        tasks_.start(
            TaskKind::Market, mailbox_,
            [input = std::move(input), fetch, request](std::stop_token stop) mutable -> BackgroundEvent {
                try {
                    auto result = fetch(std::move(input), stop);
                    check_cancelled(stop);
                    return {request, std::move(result)};
                } catch (const Cancelled &) {
                    return {request, TaskFailure{TaskKind::Market, true}};
                } catch (...) {
                    return {request, TaskFailure{TaskKind::Market, false}};
                }
            });
    } catch (const std::exception &ex) {
        active_request_ = 0;
        fail(text(ex.what()));
    }
    emit statusChanged();
    emit changed();
}
void PortfolioController::cancel() {
    if (!busy())
        return;
    tasks_.cancel(TaskKind::Market);
    active_request_ = 0;
    status_ = "Загрузка отменена; предыдущие данные сохранены.";
    emit statusChanged();
    emit changed();
}
void PortfolioController::backup() {
    if (closed_ || backupBusy())
        return;
    const auto storage = storage_;
    try {
        tasks_.start(TaskKind::Backup, mailbox_, [storage](std::stop_token stop) -> BackgroundEvent {
            BackupResult result;
            try {
                result.path = storage.create_backup(stop);
            } catch (const Cancelled &) {
                result.cancelled = true;
            } catch (const std::exception &ex) {
                result.error = ex.what();
            }
            return {0, std::move(result)};
        });
    } catch (const std::exception &ex) {
        fail(text(ex.what()));
    }
    emit statusChanged();
    emit changed();
}
void PortfolioController::openDataFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(pathText(storage_.root())));
}
void PortfolioController::drain() {
    for (auto &event : mailbox_->take()) {
        if (auto *result = std::get_if<MarketRefreshResult>(&event.data)) {
            if (event.request != active_request_ || active_request_ == 0)
                continue;
            active_request_ = 0;
            if (result->cancelled) {
                status_ = "Загрузка отменена.";
                emit statusChanged();
                emit changed();
                continue;
            }
            market_.apply(std::move(*result));
            const auto fresh = std::count_if(market_.instruments().begin(), market_.instruments().end(),
                                             [](const auto &i) { return !i.quote_stale && i.quote.last; });
            status_ = "Котировки: " + QString::number(fresh) + " / " +
                      QString::number(market_.instruments().size()) + " · проверка " +
                      QTime::currentTime().toString("HH:mm:ss");
            if (fresh == 0) {
                market_error_ = "MOEX не вернул актуальные котировки. История и операции сохранены.";
                error_ = market_error_;
            } else {
                if (error_ == market_error_)
                    error_.clear();
                market_error_.clear();
            }
            rebuild();
            emit marketDataChanged();
        } else if (auto *backupResult = std::get_if<BackupResult>(&event.data)) {
            if (!backupResult->error.empty())
                fail("Резервная копия: " + text(backupResult->error));
            else if (!backupResult->cancelled)
                status_ = "Копия создана: " + pathText(backupResult->path);
            emit statusChanged();
            emit changed();
        } else if (auto *failure = std::get_if<TaskFailure>(&event.data)) {
            if (failure->kind == TaskKind::Market && active_request_ != 0 &&
                event.request == active_request_) {
                active_request_ = 0;
                status_ = "Обновление не завершено; предыдущие данные сохранены.";
                if (!failure->cancelled) {
                    market_error_ = "Источник недоступен или загрузка завершилась ошибкой. Подробности — в "
                                    "журнале диагностики.";
                    error_ = market_error_;
                }
            }
            emit statusChanged();
            emit changed();
        }
    }
}
void PortfolioController::loadDemo() {
    cancel();
    tasks_.shutdown();
    mailbox_->take();
    writable_ = false;
    demo_ = true;
    free_cash_ = 0;
    cash_valid_ = true;
    cash_error_.clear();
    error_.clear();
    PortfolioJournal demoJournal;
    market_.instruments().clear();
    std::vector<Candle> demoIndex;
    const char *names[] = {"SBER", "GAZP", "LKOH", "YDEX"};
    const double entry[] = {265.4, 153.7, 6410, 3520}, current[] = {282.15, 146.32, 6845, 3560},
                 quantity[] = {500, 400, 100, 80};
    const auto stamp = observation_time(QDateTime::currentSecsSinceEpoch());
    for (int i = 0; i < 4; ++i) {
        demoJournal.transactions.push_back(
            {"2026-10-01", names[i], PortfolioSide::Buy, quantity[i], entry[i], 0});
        Instrument item;
        item.ticker = names[i];
        item.quote = {names[i], current[i], 0, stamp.substr(11), stamp};
        item.quote_stale = false;
        double previous = current[i] * 0.75;
        QDate date = QDate::currentDate().addDays(-460);
        int n = 0;
        while (n < 320) {
            date = date.addDays(1);
            if (date.dayOfWeek() > 5)
                continue;
            const double close = current[i] * (0.75 + 0.25 * n / 319.0 + 0.035 * std::sin(n / 9.0 + i));
            item.candles.push_back({date.toString(Qt::ISODate).toStdString(), previous,
                                    std::max(previous, close) * 1.012, std::min(previous, close) * 0.988,
                                    close, n % 31 == 0 ? 380000.0 : 100000.0 + 10000 * std::sin(n / 7.0)});
            previous = close;
            ++n;
        }
        item.candles_stale = false;
        item.candles_checked_at = "Демонстрационная история";
        if (i == 0)
            for (std::size_t k = 0; k < item.candles.size(); ++k) {
                const auto &c = item.candles[k];
                const auto close = 2500.0 + k * 1.2 + 45 * std::sin(k / 11.0);
                demoIndex.push_back({c.date, close, close + 12, close - 12, close, 1000});
            }
        market_.instruments().push_back(std::move(item));
    }
    demoJournal.transactions = identify_portfolio(std::move(demoJournal.transactions));
    for (const auto &t : demoJournal.transactions)
        demoJournal.sequence_watermark = std::max(demoJournal.sequence_watermark, t.sequence);
    market_.demoIndex(std::move(demoIndex));
    portfolio_.demo(std::move(demoJournal));
    status_ = "Запись отключена; реальный профиль не используется.";
    workspace_.state().ticker = "SBER";
    rebuild();
    emit journalChanged();
    emit marketDataChanged();
    emit writableChanged();
}
