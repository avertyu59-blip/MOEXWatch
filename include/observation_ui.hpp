#pragma once
#ifdef _WIN32
#include <windows.h>
#include "observation.hpp"
#include <functional>
namespace moexwatch {
struct ObservationBindings {
 std::function<std::vector<PriceAlert>()> rules;
 std::function<std::vector<SignalEvent>()> events;
 std::function<std::vector<std::string>()> tickers;
 std::function<std::string()> error;
 std::function<void(const std::vector<PriceAlert>&)> save_rules;
 std::function<void(const std::string&)> open_ticker;
};
HWND create_observation_page(HWND parent,HFONT font,ObservationBindings bindings);
void refresh_observation_page(HWND page);
}
#endif
