#pragma once
#include "market_types.hpp"
namespace moexwatch {
bool valid_ticker(std::string_view ticker);
std::string format_price(double value);
}
