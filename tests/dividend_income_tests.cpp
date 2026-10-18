#include "dividend_income.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace moexwatch;
void check(bool value) {
  if (!value)
    throw std::runtime_error("dividend income test failed");
}
int main() {
  DividendEvent e;
  e.record_date = "2026-10-16";
  e.period = "2025";
  e.amount_per_share = 10;
  auto key = dividend_event_key("SBER", e);
  e.amount_per_share = 20;
  check(key == dividend_event_key("SBER", e));
  auto split = allocate_dividend_income({{"one", 2, 0}, {"two", 1, 0}}, 30, 3);
  check(split && std::abs(split->at("one") - 18) < 1e-9 &&
        std::abs(split->at("two") - 9) < 1e-9);
  check(!allocate_dividend_income({}, 30, 3));
  check(!allocate_dividend_income({{"one", 2, 0}, {"one", 1, 0}}, 30, 3));
  check(!allocate_dividend_income({{"one", 2, 0}}, 30, 31));
  check(!allocate_dividend_income({{"one", 2, 0}},
                                  std::numeric_limits<double>::infinity(), 0));
  check(!allocate_dividend_income(
      {{"one", std::numeric_limits<double>::max(), 0}}, 30, 0));
}
