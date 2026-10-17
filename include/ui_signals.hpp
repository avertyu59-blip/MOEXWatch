#pragma once
#ifdef _WIN32
#include <windows.h>
#include "table_sort_win.hpp"
#include <vector>
#include <string>
#include "market_core.hpp"
#include "observation.hpp"
#include <shellapi.h>

namespace moexwatch::ui {
struct SignalsState {
    TableSort sort{-1,true};
    bool updating{};
    HWND observation_page{};
    HWND list{};
    HWND recent_events{};
    HWND summary{};
    HWND oversold_value{};
    HWND overbought_value{};
    HWND target_value{};
    HWND interval{};
    std::vector<HWND> controls;
    SignalSettings settings;
    ObservationEngine observer;
    std::vector<SignalEvent> history;
    std::string observation_error;
    std::string observation_status="Первый свежий ответ задаёт исходное состояние без уведомления.";
    bool observation_writable=true;
    bool history_dirty=false;
    bool baseline_ready = false;
    NOTIFYICONDATAW notification_icon{};
    bool notification_icon_ready = false;
};
}
#endif
