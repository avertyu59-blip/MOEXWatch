#include "purchase_plan.hpp"
#include "domain_common.hpp"
#include <algorithm>
#include <cmath>
namespace moexwatch {
std::optional<PurchasePlan> calculate_purchase_plan(const PurchasePlanInput &i) {
    for (double n : {i.entry, i.budget, i.lot_size, i.fee_percent, i.stop, i.target, i.risk_budget})
        if (!std::isfinite(n))
            return {};
    if (!valid_ticker(i.ticker) || i.entry <= 0 || i.budget <= 0 || i.lot_size < 1 || i.lot_size > 1e9 ||
        std::floor(i.lot_size) != i.lot_size || i.fee_percent < 0 || i.fee_percent >= 100 || i.stop <= 0 ||
        i.stop >= i.entry || i.target <= i.entry || i.risk_budget <= 0)
        return {};
    const double fee = i.fee_percent / 100;
    const double lot_cost = i.entry * i.lot_size * (1 + fee);
    const double lot_risk = i.lot_size * (i.entry - i.stop + fee * (i.entry + i.stop));
    if (!std::isfinite(lot_cost) || !std::isfinite(lot_risk) || lot_cost <= 0 || lot_risk <= 0)
        return {};
    PurchasePlan p;
    p.lots = std::min(std::floor(i.budget / lot_cost), std::floor(i.risk_budget / lot_risk));
    if (!std::isfinite(p.lots) || p.lots < 1 || p.lots > 1e12)
        return {};
    p.quantity = p.lots * i.lot_size;
    p.cost = p.quantity * i.entry;
    p.commission = p.cost * fee;
    p.total = p.cost + p.commission;
    p.loss = p.quantity * (i.entry - i.stop) + p.commission + p.quantity * i.stop * fee;
    p.profit = p.quantity * (i.target - i.entry) - p.commission - p.quantity * i.target * fee;
    p.ratio = p.profit / p.loss;
    p.risk_percent = p.loss / i.budget * 100;
    for (double n : {p.quantity, p.cost, p.commission, p.total, p.loss, p.profit, p.ratio, p.risk_percent})
        if (!std::isfinite(n))
            return {};
    // Tolerance is limited to floating-point evaluation, not an extra trading budget.
    if (p.total > i.budget + std::abs(i.budget) * 1e-12 ||
        p.loss > i.risk_budget + std::abs(i.risk_budget) * 1e-12)
        return {};
    return p;
}
} // namespace moexwatch
