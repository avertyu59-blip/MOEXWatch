#include "dividend_income.hpp"
#include <cmath>
namespace moexwatch {
std::string dividend_event_key(std::string_view ticker,
                               const DividendEvent &event) {
  return std::string(ticker) + "|" + event.record_date + "|" + event.period;
}
std::optional<std::map<std::string, double>> allocate_dividend_income(
    const std::vector<DividendPositionEntitlement> &positions, double gross,
    double tax) {
  if (!std::isfinite(gross) || !std::isfinite(tax) || gross <= 0 || tax < 0 ||
      tax > gross)
    return {};
  long double quantity = 0;
  std::map<std::string, double> result;
  for (const auto &p : positions) {
    if (p.position_id.empty() || !std::isfinite(p.quantity) ||
        p.quantity <= 0 || result.contains(p.position_id))
      return {};
    quantity += p.quantity;
    result[p.position_id] = 0;
  }
  if (quantity <= 0 || quantity > 1e18L)
    return {};
  double assigned = 0;
  for (std::size_t i = 0; i < positions.size(); ++i) {
    const auto &p = positions[i];
    const double value =
        i + 1 == positions.size()
            ? gross - tax - assigned
            : static_cast<double>((gross - tax) *
                                  static_cast<long double>(p.quantity) /
                                  quantity);
    if (!std::isfinite(value) || value < 0)
      return {};
    result[p.position_id] = value;
    assigned += value;
  }
  return result;
}
} // namespace moexwatch
