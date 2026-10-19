#include "purchase_plan.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace moexwatch;
void check(bool x) {
    if (!x)
        throw std::runtime_error("purchase plan test failed");
}
int main() {
    PurchasePlanInput i{"SBER", 100, 10000, 10, 1, 90, 120, 1000};
    auto p = calculate_purchase_plan(i);
    check(p && p->lots == 8 && p->quantity == 80 && p->total == 8080 && p->loss == 952 && p->profit == 1424);
    i.budget = 100;
    check(!calculate_purchase_plan(i));
    i.budget = 10000;
    i.risk_budget = 118.99;
    check(!calculate_purchase_plan(i));
    i.risk_budget = 119;
    check(calculate_purchase_plan(i)->lots == 1);
    i.risk_budget = 1000;
    i.lot_size = 1.5;
    check(!calculate_purchase_plan(i));
    i.lot_size = 10;
    i.target = std::numeric_limits<double>::infinity();
    check(!calculate_purchase_plan(i));
    i.target = 120;
    i.entry = 1e308;
    i.stop = 1e307;
    i.target = 1.5e308;
    check(!calculate_purchase_plan(i));
    i = {"SBER", 100, 1000, 1, 0, 99.9, 101, 100};
    p = calculate_purchase_plan(i);
    check(p && p->lots == 10 && p->total == 1000);
    i.ticker = "../bad";
    check(!calculate_purchase_plan(i));
}
