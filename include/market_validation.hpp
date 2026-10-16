#pragma once
#include "market_types.hpp"
namespace moexwatch {
Quote validate_quote_response(std::string_view csv,std::string_view ticker);
std::vector<Candle> validate_candle_page(std::string_view csv);
}
