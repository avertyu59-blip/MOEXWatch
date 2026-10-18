#include "strategy_controller.hpp"
#include "backtest.hpp"
#include "portfolio_controller.hpp"
#include "signal_rules.hpp"
#include <QDate>
#include <QDateTime>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
namespace {
bool decimal(QString text, double &result) {
  text = text.trimmed();
  text.replace(',', '.');
  bool ok = false;
  result = text.toDouble(&ok);
  return ok && std::isfinite(result);
}
QString str(const std::string &s) { return QString::fromStdString(s); }
bool validDate(QString value) {
  return value.isEmpty() ||
         (QDate::fromString(value, Qt::ISODate).isValid() &&
          QDate::fromString(value, Qt::ISODate).toString(Qt::ISODate) == value);
}
} // namespace
StrategyController::StrategyController(PortfolioController *market,
                                       QObject *parent)
    : QObject(parent), market_(market) {
  timer_.setInterval(20);
  connect(&timer_, &QTimer::timeout, this, &StrategyController::drain);
  if (market) {
    connect(market, &PortfolioController::chartChanged, this,
            &StrategyController::invalidate);
    connect(market, &PortfolioController::selectionChanged, this,
            &StrategyController::invalidate);
  }
}
StrategyController::~StrategyController() {
  timer_.stop();
  if (pending_.valid())
    pending_.wait();
}
void StrategyController::invalidate() {
  invalidated_ = true;
  if (!result_.isEmpty()) {
    result_["outdated"] = true;
    status_ = QStringLiteral(
        "Данные или выбранная бумага изменились — запустите тест заново.");
    emit changed();
  }
}
void StrategyController::setDividendSource(
    std::function<
        std::vector<moexwatch::BacktestCashDividend>(std::string_view)>
        source) {
  dividendSource_ = std::move(source);
  invalidateDividendData();
}
void StrategyController::invalidateDividendData() { invalidate(); }
bool StrategyController::fail(QString message) {
  error_ = std::move(message);
  emit changed();
  return false;
}
void StrategyController::clearError() {
  if (error_.isEmpty())
    return;
  error_.clear();
  emit changed();
}
bool StrategyController::run(QString capitalText, QString feeText, QString from,
                             QString to, QVariantMap rules) {
  if (running_)
    return false;
  error_.clear();
  if (!market_)
    return fail(QStringLiteral("Источник рыночных данных недоступен."));
  double capital = 0, fee = 0;
  if (!decimal(capitalText, capital) || capital <= 0 || capital > 1e12)
    return fail(QStringLiteral(
        "Капитал должен быть больше нуля и не превышать 1 трлн ₽."));
  if (!decimal(feeText, fee) || fee < 0 || fee > 10)
    return fail(
        QStringLiteral("Комиссия за каждую сторону сделки: от 0 до 10%."));
  from = from.trimmed();
  to = to.trimmed();
  if (!validDate(from) || !validDate(to) ||
      (!from.isEmpty() && !to.isEmpty() && from > to))
    return fail(QStringLiteral(
        "Задайте корректные даты ГГГГ-ММ-ДД; начало не позже окончания."));
  moexwatch::SignalSettings settings;
  settings.rsi_oversold_enabled = rules.value("oversoldEnabled", true).toBool();
  settings.rsi_overbought_enabled =
      rules.value("overboughtEnabled", false).toBool();
  settings.above_sma200_enabled = rules.value("aboveSma200", false).toBool();
  settings.near_target_enabled = rules.value("nearTarget", false).toBool();
  settings.require_all = rules.value("requireAll", false).toBool();
  if (!decimal(rules.value("oversoldBelow", 30).toString(),
               settings.rsi_oversold_below) ||
      !decimal(rules.value("overboughtAbove", 70).toString(),
               settings.rsi_overbought_above) ||
      !decimal(rules.value("nearPercent", 5).toString(),
               settings.near_target_percent) ||
      !moexwatch::valid_signal_settings(settings))
    return fail(QStringLiteral("Пороги RSI и расстояние до цели должны "
                               "находиться в пределах 0–100%."));
  if (!settings.rsi_oversold_enabled && !settings.rsi_overbought_enabled &&
      !settings.above_sma200_enabled && !settings.near_target_enabled)
    return fail(QStringLiteral("Включите хотя бы одно условие стратегии."));
  const auto ticker = market_->selectedTicker();
  const auto instruments = market_->instrumentsSnapshot();
  auto found = std::find_if(
      instruments.begin(), instruments.end(),
      [&](const auto &item) { return str(item.ticker) == ticker; });
  if (found == instruments.end())
    return fail(QStringLiteral("Выберите бумагу в разделе «Обзор»."));
  if (settings.near_target_enabled && !found->target)
    return fail(QStringLiteral(
        "Для условия «Близко к цели» сначала задайте целевую цену в обзоре."));
  auto stock = found->candles;
  auto index = market_->indexSnapshot();
  auto filter = [&](auto &candles) {
    candles.erase(std::remove_if(candles.begin(), candles.end(),
                                 [&](const auto &c) {
                                   QString date = str(c.date).left(10);
                                   return (!from.isEmpty() && date < from) ||
                                          (!to.isEmpty() && date > to);
                                 }),
                  candles.end());
  };
  filter(stock);
  filter(index);
  if (stock.size() > 5000 || index.size() > 5000)
    return fail(
        QStringLiteral("Выберите период не длиннее 5000 торговых сессий."));
  if (stock.size() < 2 || index.size() < 2)
    return fail(QStringLiteral("Недостаточно истории акции или IMOEX за "
                               "выбранный период. Обновите данные."));
  auto valid = [](const auto &candles) {
    for (const auto &c : candles)
      if (!validDate(str(c.date).left(10)) || c.date.size() < 10)
        return false;
    return true;
  };
  if (!valid(stock) || !valid(index))
    return fail(QStringLiteral(
        "В истории обнаружена некорректная дата; тест не выполнен."));
  std::set<std::string> dates;
  for (const auto &c : index)
    dates.insert(c.date.substr(0, 10));
  std::size_t common = 0;
  for (const auto &c : stock)
    if (dates.contains(c.date.substr(0, 10)))
      ++common;
  if (common < 2)
    return fail(QStringLiteral(
        "У акции и IMOEX нет как минимум двух общих торговых дат."));
  const bool includeDividends = rules.value("includeDividends", false).toBool();
  std::vector<moexwatch::BacktestCashDividend> dividends;
  if (includeDividends) {
    if (!dividendSource_)
      return fail(
          QStringLiteral("Источник фактических дивидендов не подключён."));
    try {
      dividends = dividendSource_(ticker.toStdString());
    } catch (const std::exception &) {
      return fail(QStringLiteral("Не удалось получить фактические выплаты."));
    }
    if (dividends.empty())
      return fail(QStringLiteral(
          "В профиле нет фактических выплат для этой бумаги. Отключите "
          "дивиденды либо добавьте подтверждённые поступления."));
  }
  const auto target = found->target;
  const bool demo = market_->demo();
  const bool stale =
      found->candles_stale ||
      !market_->analysis().value("indexError").toString().isEmpty() ||
      QDate::fromString(str(stock.back().date).left(10), Qt::ISODate)
              .daysTo(QDate::currentDate()) > 7 ||
      QDate::fromString(str(index.back().date).left(10), Qt::ISODate)
              .daysTo(QDate::currentDate()) > 7;
  running_ = true;
  invalidated_ = false;
  status_ = QStringLiteral("Расчёт на снимке истории — интерфейс доступен.");
  try {
    pending_ = std::async(
        std::launch::async,
        [stock = std::move(stock), index = std::move(index), settings, target,
         capital, fee, ticker, demo, stale, common, includeDividends,
         dividends = std::move(dividends)]() -> Outcome {
          try {
            const auto summary = moexwatch::run_backtest(
                stock, index, settings, target, capital, fee, dividends);
            if (!summary)
              return {{},
                      QStringLiteral("История или параметры не прошли проверку "
                                     "расчётного ядра.")};
            const auto &s = summary->summary;
            QStringList conditions;
            if (settings.rsi_oversold_enabled)
              conditions << QStringLiteral("RSI < %1")
                                .arg(settings.rsi_oversold_below);
            if (settings.rsi_overbought_enabled)
              conditions << QStringLiteral("RSI > %1")
                                .arg(settings.rsi_overbought_above);
            if (settings.above_sma200_enabled)
              conditions << QStringLiteral("Цена > SMA200");
            if (settings.near_target_enabled)
              conditions << QStringLiteral("До цели %1 ₽ ≤ %2%")
                                .arg(*target)
                                .arg(settings.near_target_percent);
            QVariantMap result{
                {"ticker", ticker},
                {"rules", conditions.join(settings.require_all
                                              ? QStringLiteral(" И ")
                                              : QStringLiteral(" ИЛИ "))},
                {"capital", s.starting_capital},
                {"strategyFinal", s.strategy_final_value},
                {"benchmarkFinal", s.benchmark_final_value},
                {"strategyReturn", s.strategy_return_percent},
                {"benchmarkReturn", s.benchmark_return_percent},
                {"drawdown", s.strategy_max_drawdown_percent},
                {"trades", qulonglong(s.completed_trades)},
                {"from", str(s.start_date)},
                {"to", str(s.end_date)},
                {"commonSessions", qulonglong(common)},
                {"commission", fee},
                {"demo", demo},
                {"historyStale", stale},
                {"outdated", false},
                {"smaWarmup",
                 settings.above_sma200_enabled && stock.size() < 201},
                {"computedAt",
                 QDateTime::currentDateTime().toString(Qt::ISODate)}};
            result["includeDividends"] = includeDividends;
            result["dividendSource"] = QStringLiteral(
                "Фактические выплаты текущего профиля: неполная выборка, суммы "
                "до налога; без гипотетического налога.");
            result["dividendsReceived"] = summary->strategy_dividends_received;
            result["dividendsReceivable"] =
                summary->strategy_dividends_receivable;
            result["stockHoldFinal"] = summary->stock_buy_hold_final_value;
            result["stockHoldReturn"] = summary->stock_buy_hold_return_percent;
            result["dividendEvents"] = qulonglong(summary->dividend_events);
            return {result, {}};
          } catch (const std::exception &) {
            return {
                {},
                QStringLiteral(
                    "Ошибка расчёта стратегии. Исходные данные сохранены.")};
          }
        });
  } catch (const std::exception &) {
    running_ = false;
    return fail(QStringLiteral("Не удалось запустить рабочий поток расчёта."));
  }
  timer_.start();
  emit changed();
  return true;
}
void StrategyController::drain() {
  if (!pending_.valid() || pending_.wait_for(std::chrono::milliseconds(0)) !=
                               std::future_status::ready)
    return;
  timer_.stop();
  running_ = false;
  auto outcome = pending_.get();
  if (!outcome.error.isEmpty()) {
    error_ = outcome.error;
    status_ = QStringLiteral("Тест завершился без результата.");
  } else {
    result_ = std::move(outcome.result);
    result_["outdated"] = invalidated_;
    status_ = invalidated_
                  ? QStringLiteral("Тест рассчитан; данные изменились во время "
                                   "расчёта. Повторите тест.")
                  : QStringLiteral("Тест завершён. Результат относится к "
                                   "указанному снимку истории.");
  }
  emit changed();
}
