#include "domain_common.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
namespace moexwatch {
bool valid_ticker(std::string_view ticker) {
    if (ticker.empty() || ticker.size() > 12) return false;
    return std::all_of(ticker.begin(), ticker.end(), [](unsigned char c) { return std::isalnum(c) != 0 || c == '.' || c == '-'; });
}

std::string format_price(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(value >= 1000.0 ? 1 : 2) << value;
    return out.str();
}

}
