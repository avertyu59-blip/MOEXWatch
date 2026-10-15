#pragma once
#include "market_core.hpp"
#include <cstdint>
#include <map>
namespace moexwatch {
struct PriceAlert {
    std::uint64_t id{};
    std::string ticker;
    bool at_or_below{true};
    double level{};
    bool enabled{true};
};
struct SignalEvent {
    std::string detected_at;
    std::string ticker;
    std::string kind;
    std::string condition;
    double price{};
    std::string quote_time;
    std::string source_time;
};
class ObservationEngine {
    std::vector<PriceAlert> rules_;
    std::map<std::uint64_t,bool> price_state_;
    std::map<std::string,bool> technical_state_;
public:
    const std::vector<PriceAlert>& rules() const { return rules_; }
    void configure(std::vector<PriceAlert> rules);
    void reset_technical() { technical_state_.clear(); }
    void forget_ticker(std::string_view ticker);
    std::vector<SignalEvent> observe(const Quote& quote, bool fresh, bool technical_available,
        const SignalEvaluation& technical, std::string_view technical_mode, std::int64_t now_utc);
};
bool same_technical_rules(const SignalSettings& a, const SignalSettings& b);
bool fresh_moex_quote(const Quote& quote, std::int64_t now_utc);
std::string observation_time(std::int64_t now_utc);
std::string serialize_price_alerts(const std::vector<PriceAlert>& rules);
std::vector<PriceAlert> parse_price_alerts(std::string_view csv);
std::string serialize_signal_history(const std::vector<SignalEvent>& events);
std::vector<SignalEvent> parse_signal_history(std::string_view csv);
// Missing/nonfinite values stay at the end for either direction.
bool optional_number_less(std::optional<double> a, std::optional<double> b, bool ascending);
std::optional<double> table_number(std::string_view text);
} // namespace moexwatch
