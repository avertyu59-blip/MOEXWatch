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
        quote.source_timestamp = field(rows[i], column(header,"SYSTIME")).value_or("");
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

std::vector<DividendEvent> parse_dividends_csv(std::string_view csv) {
    const auto rows = parse_csv(csv);
    std::vector<DividendEvent> events;
    const auto header_it = std::find_if(rows.begin(), rows.end(), [](const auto& row) {
        return column(row, "REGISTRYCLOSEDATE") && column(row, "VALUE");
    });
    if (header_it == rows.end()) return events;
    const auto& header = *header_it;
    const auto first_data = static_cast<std::size_t>(header_it - rows.begin()) + 1;
    const auto record_date = column(header, "REGISTRYCLOSEDATE");
    const auto amount = column(header, "VALUE");
    const auto currency = column(header, "CURRENCYID");
    for (std::size_t i = first_data; i < rows.size(); ++i) {
        const auto date = field(rows[i], record_date);
        const auto value = number(field(rows[i], amount).value_or(""));
        if (!date || date->size() < 10 || !value || *value <= 0.0) continue;
        const std::string normalized_date = date->substr(0, 10);
        if (normalized_date[4] != '-' || normalized_date[7] != '-') continue;
        const std::string currency_code = upper(trim(field(rows[i], currency).value_or("")));
        DividendEvent event;
        event.record_date = normalized_date; event.amount_per_share = *value; event.currency = currency_code;
        events.push_back(std::move(event));
    }
    std::stable_sort(events.begin(), events.end(), [](const auto& left, const auto& right) {
        return left.record_date < right.record_date;
    });
    return events;
}

} // namespace moexwatch
