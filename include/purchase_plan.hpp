#pragma once
#include <optional>
#include <string>
namespace moexwatch {
struct PurchasePlanInput {
    std::string ticker;
    double entry{}, budget{}, lot_size{}, fee_percent{}, stop{}, target{}, risk_budget{};
};
struct PurchasePlan {
    double lots{}, quantity{}, cost{}, commission{}, total{}, loss{}, profit{}, ratio{}, risk_percent{};
};
// Scenario calculation only: no quotes, broker orders, UI or persistence.
std::optional<PurchasePlan> calculate_purchase_plan(const PurchasePlanInput &input);
} // namespace moexwatch
