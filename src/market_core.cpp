#include "market_core.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <numeric>
#include <map>
#include <sstream>

namespace moexwatch {
namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::optional<double> number(std::string value) {
    value = trim(std::move(value));
    if (value.empty() || value == "null" || value == "NA" || value == "-" || value == "nan") return std::nullopt;
    std::replace(value.begin(), value.end(), ',', '.');
    double result{};
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec != std::errc{} || end != value.data() + value.size() || !std::isfinite(result)) return std::nullopt;
    return result;
}

std::optional<std::size_t> column(const std::vector<std::string>& header, std::string_view name) {
    for (std::size_t i = 0; i < header.size(); ++i) if (upper(trim(header[i])) == name) return i;
    return std::nullopt;
}

std::optional<std::string> field(const std::vector<std::string>& row, std::optional<std::size_t> index) {
    if (!index || *index >= row.size()) return std::nullopt;
    return row[*index];
}

std::string date_key(const Candle& candle) {
    return candle.date.size() >= 10 ? candle.date.substr(0, 10) : std::string{};
}

} // namespace

std::vector<std::vector<std::string>> parse_csv(std::string_view csv, char delimiter) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string cell;
    bool quoted = false;
    for (std::size_t i = 0; i < csv.size(); ++i) {
        const char ch = csv[i];
        if (quoted) {
            if (ch == '"' && i + 1 < csv.size() && csv[i + 1] == '"') { cell.push_back('"'); ++i; }
            else if (ch == '"') quoted = false;
            else cell.push_back(ch);
        } else if (ch == '"' && cell.empty()) quoted = true;
        else if (ch == delimiter) { row.push_back(std::move(cell)); cell.clear(); }
        else if (ch == '\n' || ch == '\r') {
            if (ch == '\r' && i + 1 < csv.size() && csv[i + 1] == '\n') ++i;
            row.push_back(std::move(cell)); cell.clear();
            if (std::any_of(row.begin(), row.end(), [](const auto& item) { return !item.empty(); })) rows.push_back(std::move(row));
            row.clear();
        } else cell.push_back(ch);
    }
    row.push_back(std::move(cell));
    if (std::any_of(row.begin(), row.end(), [](const auto& item) { return !item.empty(); })) rows.push_back(std::move(row));
    return rows;
}

std::optional<Quote> parse_quote_csv(std::string_view csv, std::string_view ticker) {
    const auto rows = parse_csv(csv);
    if (rows.size() < 2) return std::nullopt;
    const auto header_it = std::find_if(rows.begin(), rows.end(), [](const auto& row) {
        return column(row, "SECID").has_value() && column(row, "LAST").has_value();
    });
    if (header_it == rows.end()) return std::nullopt;
    const auto& header = *header_it;
    const auto first_data = static_cast<std::size_t>(header_it - rows.begin()) + 1;
    const auto secid = column(header, "SECID");
    const auto last = column(header, "LAST");
    const auto change = column(header, "LASTCHANGEPRCNT");
    const auto updated = column(header, "UPDATETIME");
    if (!last) return std::nullopt;
    for (std::size_t i = first_data; i < rows.size(); ++i) {
        if (secid) {
            const auto id = field(rows[i], secid);
            if (!id || upper(trim(*id)) != upper(std::string(ticker))) continue;
        }
        Quote quote;
        quote.ticker = upper(std::string(ticker));
        quote.last = number(field(rows[i], last).value_or(""));
        if (change) quote.change_percent = number(field(rows[i], change).value_or(""));
        quote.updated_at = field(rows[i], updated).value_or("");
        return quote;
    }
    return std::nullopt;
}

bool has_candle_header(std::string_view csv) {
    const auto rows = parse_csv(csv);
    return std::any_of(rows.begin(), rows.end(), [](const auto& row) {
        return column(row, "BEGIN") && column(row, "OPEN") && column(row, "HIGH") &&
               column(row, "LOW") && column(row, "CLOSE");
    });
}

std::vector<Candle> parse_candles_csv(std::string_view csv) {
    const auto rows = parse_csv(csv);
    std::vector<Candle> candles;
    if (rows.size() < 2) return candles;
    const auto header_it = std::find_if(rows.begin(), rows.end(), [](const auto& row) {
        return column(row, "BEGIN").has_value() && column(row, "CLOSE").has_value();
    });
    if (header_it == rows.end()) return candles;
    const auto& header = *header_it;
    const auto first_data = static_cast<std::size_t>(header_it - rows.begin()) + 1;
    const auto date = column(header, "BEGIN");
    const auto open = column(header, "OPEN");
    const auto high = column(header, "HIGH");
    const auto low = column(header, "LOW");
    const auto close = column(header, "CLOSE");
    const auto volume = column(header, "VOLUME");
    if (!date || !open || !high || !low || !close) return candles;
    for (std::size_t i = first_data; i < rows.size(); ++i) {
        const auto o = number(field(rows[i], open).value_or(""));
        const auto h = number(field(rows[i], high).value_or(""));
        const auto l = number(field(rows[i], low).value_or(""));
        const auto c = number(field(rows[i], close).value_or(""));
        if (!o || !h || !l || !c || *h < *l || *c <= 0.0) continue;
        candles.push_back({field(rows[i], date).value_or(""), *o, *h, *l, *c,
                           number(field(rows[i], volume).value_or("" )).value_or(0.0)});
    }
    return candles;
}

std::vector<std::optional<double>> sma_series(const std::vector<Candle>& candles, std::size_t period) {
    std::vector<std::optional<double>> values(candles.size());
    if (period == 0) return values;
    double sum = 0.0;
    for (std::size_t i = 0; i < candles.size(); ++i) {
        sum += candles[i].close;
        if (i >= period) sum -= candles[i - period].close;
        if (i + 1 >= period) values[i] = sum / static_cast<double>(period);
    }
    return values;
}

std::vector<std::optional<double>> rsi_series(const std::vector<Candle>& candles, std::size_t period) {
    std::vector<std::optional<double>> values(candles.size());
    if (period == 0 || candles.size() <= period) return values;
    double gain = 0.0, loss = 0.0;
    for (std::size_t i = 1; i <= period; ++i) {
        const double change = candles[i].close - candles[i - 1].close;
        gain += std::max(change, 0.0); loss += std::max(-change, 0.0);
    }
    const double length = static_cast<double>(period);
    gain /= length; loss /= length;
    const auto value = [&]() { return loss == 0.0 ? (gain == 0.0 ? 50.0 : 100.0) : 100.0 - 100.0 / (1.0 + gain / loss); };
    values[period] = value();
    for (std::size_t i = period + 1; i < candles.size(); ++i) {
        const double change = candles[i].close - candles[i - 1].close;
        gain = (gain * (length - 1.0) + std::max(change, 0.0)) / length;
        loss = (loss * (length - 1.0) + std::max(-change, 0.0)) / length;
        values[i] = value();
    }
    return values;
}

std::vector<std::optional<double>> atr_series(const std::vector<Candle>& candles, std::size_t period) {
    std::vector<std::optional<double>> values(candles.size());
    if (period == 0 || candles.size() < period) return values;

    const auto true_range = [&](std::size_t index) {
        const auto& candle = candles[index];
        if (index == 0) return candle.high - candle.low;
        const double previous_close = candles[index - 1].close;
        return std::max({candle.high - candle.low,
                         std::abs(candle.high - previous_close),
                         std::abs(candle.low - previous_close)});
    };

    const double length = static_cast<double>(period);
    double average = 0.0;
    for (std::size_t i = 0; i < period; ++i) average += true_range(i);
    average /= length;
    values[period - 1] = average;
    for (std::size_t i = period; i < candles.size(); ++i) {
        average = (average * (length - 1.0) + true_range(i)) / length;
        values[i] = average;
    }
    return values;
}

std::vector<std::optional<double>> volume_ratio_series(const std::vector<Candle>& candles, std::size_t period) {
    std::vector<std::optional<double>> values(candles.size());
    if (period == 0 || candles.size() <= period) return values;

    double sum = 0.0;
    for (std::size_t i = 0; i < candles.size(); ++i) {
        if (i > period) sum -= candles[i - period - 1].volume;
        if (i > 0) sum += candles[i - 1].volume;
        if (i >= period) {
            const double average = sum / static_cast<double>(period);
            if (average > 0.0 && candles[i].volume >= 0.0)
                values[i] = candles[i].volume / average;
        }
    }
    return values;
}

NormalizedComparison normalized_comparison_series(const std::vector<Candle>& stock,
                                                   const std::vector<Candle>& benchmark,
                                                   std::size_t start_index) {
    NormalizedComparison result{std::vector<std::optional<double>>(stock.size()),
                                std::vector<std::optional<double>>(stock.size())};
    if (start_index >= stock.size()) return result;

    std::map<std::string, double> benchmark_by_date;
    for (const auto& candle : benchmark) {
        const auto key = date_key(candle);
        if (!key.empty() && std::isfinite(candle.close) && candle.close > 0.0)
            benchmark_by_date.try_emplace(key, candle.close);
    }

    std::optional<double> stock_base;
    std::optional<double> benchmark_base;
    for (std::size_t i = start_index; i < stock.size(); ++i) {
        const auto key = date_key(stock[i]);
        const auto found = benchmark_by_date.find(key);
        if (key.empty() || found == benchmark_by_date.end() ||
            !std::isfinite(stock[i].close) || stock[i].close <= 0.0) continue;
        stock_base = stock[i].close;
        benchmark_base = found->second;
        break;
    }
    if (!stock_base || !benchmark_base) return result;

    for (std::size_t i = start_index; i < stock.size(); ++i) {
        const auto key = date_key(stock[i]);
        const auto found = benchmark_by_date.find(key);
        if (key.empty() || found == benchmark_by_date.end() ||
            !std::isfinite(stock[i].close) || stock[i].close <= 0.0) continue;
        result.stock[i] = stock[i].close / *stock_base * 100.0;
        result.benchmark[i] = found->second / *benchmark_base * 100.0;
    }
    return result;
}

Analysis analyze(const std::vector<Candle>& candles) {
    Analysis result;
    if (candles.empty()) { result.technical_note = "Недостаточно данных"; return result; }
    result.sma20 = sma_series(candles, 20).back();
    result.sma50 = sma_series(candles, 50).back();
    result.sma200 = sma_series(candles, 200).back();
    result.rsi14 = rsi_series(candles).back();
    result.atr14 = atr_series(candles).back();
    if (result.atr14 && candles.back().close > 0.0)
        result.atr_percent = *result.atr14 / candles.back().close * 100.0;
    if (candles.size() >= 2 && candles.front().close > 0.0)
        result.period_change_percent = (candles.back().close / candles.front().close - 1.0) * 100.0;
    result.volume_ratio20 = volume_ratio_series(candles).back();
    double peak = candles.front().high;
    for (const auto& candle : candles) peak = std::max(peak, candle.high);
    if (peak > 0.0) result.drawdown_percent = (candles.back().close / peak - 1.0) * 100.0;
    if (!result.rsi14) result.technical_note = "Для RSI14 нужно не менее 15 свечей";
    else if (*result.rsi14 > 70.0) result.technical_note = "Перекупленность: RSI14 > 70";
    else if (*result.rsi14 < 30.0) result.technical_note = "Перепроданность: RSI14 < 30";
    else result.technical_note = "RSI14 в диапазоне 30–70";
    return result;
}

bool matches_filter(const Analysis& analysis, std::optional<double> last, std::optional<double> target,
                    bool stale, WatchFilter filter) {
    if (filter == WatchFilter::All) return true;
    if (stale || !last) return false;
    switch (filter) {
    case WatchFilter::Oversold: return analysis.rsi14 && *analysis.rsi14 < 30.0;
    case WatchFilter::Overbought: return analysis.rsi14 && *analysis.rsi14 > 70.0;
    case WatchFilter::AboveSma200: return analysis.sma200 && *last > *analysis.sma200;
    case WatchFilter::NearTarget: return target && *target > 0.0 && std::abs(*last / *target - 1.0) <= 0.05 + 1e-12;
    case WatchFilter::All: return true;
    }
    return false;
}

bool valid_ticker(std::string_view ticker) {
    if (ticker.empty() || ticker.size() > 12) return false;
    return std::all_of(ticker.begin(), ticker.end(), [](unsigned char c) { return std::isalnum(c) != 0 || c == '.' || c == '-'; });
}

std::string format_price(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(value >= 1000.0 ? 1 : 2) << value;
    return out.str();
}

bool valid_trade(const Trade& trade) noexcept {
    return valid_ticker(trade.ticker) && std::isfinite(trade.quantity) && trade.quantity > 0.0 &&
           std::isfinite(trade.buy_price) && trade.buy_price > 0.0 &&
           std::isfinite(trade.sell_price) && trade.sell_price > 0.0 &&
           std::isfinite(trade.commission) && trade.commission >= 0.0 &&
           std::isfinite(trade.tax_rate_percent) && trade.tax_rate_percent >= 0.0 &&
           trade.tax_rate_percent <= 100.0;
}

std::optional<TradeResult> calculate_trade(const Trade& trade) noexcept {
    if (!valid_trade(trade)) return std::nullopt;
    TradeResult result;
    result.gross_profit = (trade.sell_price - trade.buy_price) * trade.quantity;
    result.profit_after_commission = result.gross_profit - trade.commission;
    const double taxable_profit = std::max(0.0, result.profit_after_commission);
    result.tax = taxable_profit * trade.tax_rate_percent / 100.0;
    result.net_profit = result.profit_after_commission - result.tax;
    if (!std::isfinite(result.gross_profit) || !std::isfinite(result.profit_after_commission) ||
        !std::isfinite(result.tax) || !std::isfinite(result.net_profit)) return std::nullopt;
    return result;
}

bool valid_signal_settings(const SignalSettings& settings) noexcept {
    const auto threshold = [](double value) { return std::isfinite(value) && value >= 0.0 && value <= 100.0; };
    const bool interval_valid = settings.auto_refresh_minutes == 15 || settings.auto_refresh_minutes == 30 ||
                                settings.auto_refresh_minutes == 60;
    return threshold(settings.rsi_oversold_below) && threshold(settings.rsi_overbought_above) &&
           threshold(settings.near_target_percent) && interval_valid;
}

SignalEvaluation evaluate_signals(const Analysis& analysis, std::optional<double> last,
                                  std::optional<double> target, bool quote_stale,
                                  bool candles_stale, const SignalSettings& settings) {
    SignalEvaluation result;
    if (!valid_signal_settings(settings) || quote_stale || candles_stale || !last || !std::isfinite(*last) || *last <= 0.0)
        return result;
    struct Condition { bool matched; std::string reason; };
    std::vector<Condition> conditions;
    const bool has_rsi = analysis.rsi14 && std::isfinite(*analysis.rsi14);
    if (settings.rsi_oversold_enabled) {
        const bool matched = has_rsi && *analysis.rsi14 < settings.rsi_oversold_below;
        std::ostringstream reason;
        reason << std::fixed << std::setprecision(1) << "RSI14 " << (has_rsi ? *analysis.rsi14 : 0.0)
               << " < " << settings.rsi_oversold_below;
        conditions.push_back({matched, reason.str()});
    }
    if (settings.rsi_overbought_enabled) {
        const bool matched = has_rsi && *analysis.rsi14 > settings.rsi_overbought_above;
        std::ostringstream reason;
        reason << std::fixed << std::setprecision(1) << "RSI14 " << (has_rsi ? *analysis.rsi14 : 0.0)
               << " > " << settings.rsi_overbought_above;
        conditions.push_back({matched, reason.str()});
    }
    if (settings.above_sma200_enabled) {
        const bool available = analysis.sma200 && std::isfinite(*analysis.sma200);
        const bool matched = available && *last > *analysis.sma200;
        std::ostringstream reason;
        reason << std::fixed << std::setprecision(2) << "Цена " << *last << (matched ? " выше SMA200 " : " не выше SMA200 ")
               << (available ? *analysis.sma200 : 0.0);
        conditions.push_back({matched, reason.str()});
    }
    if (settings.near_target_enabled) {
        const bool available = target && std::isfinite(*target) && *target > 0.0;
        const double distance = available ? std::abs(*last / *target - 1.0) * 100.0 : 0.0;
        const bool matched = available && distance <= settings.near_target_percent + 1e-12;
        std::ostringstream reason;
        reason << std::fixed << std::setprecision(2) << "До цели " << distance << "% (лимит "
               << settings.near_target_percent << "%)";
        conditions.push_back({matched, reason.str()});
    }
    if (conditions.empty()) return result;
    bool all_available_and_matched = true;
    bool at_least_one_match = false;
    for (const auto& condition : conditions) {
        if (condition.matched) {
            result.reasons.push_back(condition.reason);
        } else {
            all_available_and_matched = false;
        }
        at_least_one_match = at_least_one_match || condition.matched;
    }
    result.matched = settings.require_all ? all_available_and_matched : at_least_one_match;
    if (!result.matched) result.reasons.clear();
    return result;
}



namespace {
bool valid_iso_date(std::string_view date) noexcept {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    const auto parse_part = [&](std::size_t begin, std::size_t count) -> int {
        int value{};
        const auto* first = date.data() + begin;
        const auto* last = first + count;
        const auto [end, error] = std::from_chars(first, last, value);
        return error == std::errc{} && end == last ? value : -1;
    };
    const int y = parse_part(0, 4), m = parse_part(5, 2), d = parse_part(8, 2);
    return y >= 1900 && std::chrono::year_month_day{std::chrono::year{y},
        std::chrono::month{static_cast<unsigned>(m)}, std::chrono::day{static_cast<unsigned>(d)}}.ok();
}
}

bool valid_portfolio_transaction(const PortfolioTransaction& transaction) noexcept {
    return valid_iso_date(transaction.date) && valid_ticker(transaction.ticker) &&
           std::isfinite(transaction.quantity) && transaction.quantity > 0.0 &&
           std::isfinite(transaction.price) && transaction.price > 0.0 &&
           std::isfinite(transaction.commission) && transaction.commission >= 0.0 &&
           (transaction.side == PortfolioSide::Buy || transaction.side == PortfolioSide::Sell);
}

bool valid_position_levels(const PositionLevels& levels) noexcept {
    const auto valid_level = [](const std::optional<double>& value) {
        return !value || (std::isfinite(*value) && *value > 0.0);
    };
    return valid_ticker(levels.ticker) && valid_iso_date(levels.opened_on) &&
           valid_level(levels.stop_loss) && valid_level(levels.take_profit);
}

std::optional<PortfolioSummary> calculate_portfolio(const std::vector<PortfolioTransaction>& transactions,
                                                    const std::vector<Quote>& quotes) {
    struct State { double quantity{}; double average_cost{}; double realized{}; std::string opened_on; };
    std::vector<PortfolioTransaction> ordered = transactions;
    for (const auto& transaction : ordered) if (!valid_portfolio_transaction(transaction)) return std::nullopt;
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        return left.date < right.date;
    });
    std::map<std::string, State> states;
    for (const auto& transaction : ordered) {
        auto& state = states[upper(transaction.ticker)];
        if (transaction.side == PortfolioSide::Buy) {
            if (state.quantity == 0.0) state.opened_on = transaction.date;
            const double old_cost = state.quantity * state.average_cost;
            const double added_cost = transaction.quantity * transaction.price + transaction.commission;
            const double new_quantity = state.quantity + transaction.quantity;
            const double new_cost = old_cost + added_cost;
            if (!std::isfinite(new_quantity) || !std::isfinite(new_cost) || new_quantity <= 0.0) return std::nullopt;
            state.quantity = new_quantity;
            state.average_cost = new_cost / new_quantity;
        } else {
            constexpr double quantity_epsilon = 1e-9;
            if (transaction.quantity > state.quantity + quantity_epsilon) return std::nullopt;
            state.realized += transaction.quantity * (transaction.price - state.average_cost) - transaction.commission;
            state.quantity -= transaction.quantity;
            if (state.quantity <= quantity_epsilon) { state.quantity = 0.0; state.average_cost = 0.0; state.opened_on.clear(); }
            if (!std::isfinite(state.realized)) return std::nullopt;
        }
    }
    std::map<std::string, std::optional<double>> last_prices;
    for (const auto& quote : quotes) {
        if (valid_ticker(quote.ticker)) last_prices[upper(quote.ticker)] =
            quote.last && std::isfinite(*quote.last) && *quote.last > 0.0 ? quote.last : std::nullopt;
    }
    PortfolioSummary result;
    for (const auto& [ticker, state] : states) {
        PortfolioPosition position;
        position.ticker = ticker;
        position.opened_on = state.opened_on;
        position.quantity = state.quantity;
        position.average_entry_price = state.average_cost;
        position.realized_profit = state.realized;
        result.total_realized_profit += state.realized;
        if (state.quantity > 0.0) {
            ++result.open_positions;
            const auto price = last_prices.find(ticker);
            if (price != last_prices.end() && price->second) {
                position.current_price = price->second;
                position.market_value = state.quantity * *price->second;
                position.unrealized_profit = *position.market_value - state.quantity * state.average_cost;
                const double basis = state.quantity * state.average_cost;
                if (basis > 0.0) position.unrealized_percent = *position.unrealized_profit / basis * 100.0;
                result.total_market_value += *position.market_value;
                result.total_unrealized_profit += *position.unrealized_profit;
                ++result.priced_positions;
            }
        }
        result.positions.push_back(std::move(position));
    }
    if (!std::isfinite(result.total_realized_profit) || !std::isfinite(result.total_market_value) ||
        !std::isfinite(result.total_unrealized_profit)) return std::nullopt;
    return result;
}

std::optional<BacktestSummary> run_backtest(const std::vector<Candle>& stock,
                                            const std::vector<Candle>& index,
                                            const SignalSettings& settings,
                                            std::optional<double> target,
                                            double starting_capital,
                                            double commission_percent_per_side) {
    if (!valid_signal_settings(settings) || !std::isfinite(starting_capital) || starting_capital <= 0.0 ||
        !std::isfinite(commission_percent_per_side) || commission_percent_per_side < 0.0 ||
        commission_percent_per_side > 10.0 ||
        (target && (!std::isfinite(*target) || *target <= 0.0))) return std::nullopt;
    const auto date_key = [](const Candle& candle) -> std::string {
        if (candle.date.size() < 10) return {};
        return candle.date.substr(0, 10);
    };
    const auto valid_series = [&](const std::vector<Candle>& candles) {
        std::string previous;
        for (const auto& candle : candles) {
            const auto date = date_key(candle);
            if (date.empty() || (!previous.empty() && date <= previous) ||
                !std::isfinite(candle.open) || candle.open <= 0.0 ||
                !std::isfinite(candle.close) || candle.close <= 0.0) return false;
            previous = date;
        }
        return true;
    };
    if (stock.size() < 2 || index.size() < 2 || !valid_series(stock) || !valid_series(index)) return std::nullopt;

    std::map<std::string, Candle> index_by_date;
    for (const auto& candle : index) index_by_date.emplace(date_key(candle), candle);
    std::vector<std::pair<Candle, Candle>> common;
    for (const auto& candle : stock) {
        const auto found = index_by_date.find(date_key(candle));
        if (found != index_by_date.end()) common.emplace_back(candle, found->second);
    }
    if (common.size() < 2) return std::nullopt;

    std::vector<Candle> aligned_stock;
    aligned_stock.reserve(common.size());
    for (const auto& pair : common) aligned_stock.push_back(pair.first);
    const auto rsi = rsi_series(aligned_stock);
    const auto sma200 = sma_series(aligned_stock, 200);
    const double fee = commission_percent_per_side / 100.0;
    double cash = starting_capital;
    double shares = 0.0;
    double peak = starting_capital;
    double max_drawdown = 0.0;
    std::size_t completed = 0;

    for (std::size_t i = 0; i + 1 < common.size(); ++i) {
        Analysis analysis;
        analysis.rsi14 = rsi[i];
        analysis.sma200 = sma200[i];
        const auto signal = evaluate_signals(analysis, common[i].first.close, target, false, false, settings);
        const double next_open = common[i + 1].first.open;
        if (shares == 0.0 && signal.matched) {
            shares = cash / (next_open * (1.0 + fee));
            cash = 0.0;
        } else if (shares > 0.0 && !signal.matched) {
            cash = shares * next_open * (1.0 - fee);
            shares = 0.0;
            ++completed;
        }
        const double mark = cash + shares * common[i + 1].first.close * (1.0 - fee);
        peak = std::max(peak, mark);
        if (peak > 0.0) max_drawdown = std::max(max_drawdown, (peak - mark) / peak * 100.0);
    }
    if (shares > 0.0) {
        cash = shares * common.back().first.close * (1.0 - fee);
        ++completed;
    }

    const double index_shares = starting_capital / (common.front().second.open * (1.0 + fee));
    const double benchmark_final = index_shares * common.back().second.close * (1.0 - fee);
    if (!std::isfinite(cash) || !std::isfinite(benchmark_final) || cash <= 0.0 || benchmark_final <= 0.0)
        return std::nullopt;
    BacktestSummary result;
    result.starting_capital = starting_capital;
    result.strategy_final_value = cash;
    result.benchmark_final_value = benchmark_final;
    result.strategy_return_percent = (cash / starting_capital - 1.0) * 100.0;
    result.benchmark_return_percent = (benchmark_final / starting_capital - 1.0) * 100.0;
    result.strategy_max_drawdown_percent = max_drawdown;
    result.completed_trades = completed;
    result.start_date = date_key(common.front().first);
    result.end_date = date_key(common.back().first);
    return result;
}

} // namespace moexwatch
