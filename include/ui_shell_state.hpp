#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vector>
#include <array>
#include <utility>
namespace moexwatch::ui {
struct ShellState {
    ~ShellState() {
        if(canvas_brush)DeleteObject(canvas_brush);
        if(surface_brush)DeleteObject(surface_brush);
        if(heading_font)DeleteObject(heading_font);
        if(metric_font)DeleteObject(metric_font);
    }
    HWND window{};
    bool dark_theme{true};
    HBRUSH canvas_brush{}, surface_brush{};
    HFONT heading_font{}, metric_font{};
    std::array<HWND, 8> navigation{};
    std::vector<std::pair<HWND, RECT>> base_layout;

    HWND backup_button{};
    HWND tabs{};
    HWND cancel_loading{};
    HWND diagnostics{};
    bool closing{};
    HFONT font{};
    int active_page{};
    std::vector<HWND> global_controls;
};
// Restores nested notification suppression even if filling a view exits early.
class ScopedViewUpdate {
    bool& flag_;
    bool previous_;
public:
    explicit ScopedViewUpdate(bool& flag):flag_(flag),previous_(flag){flag_=true;}
    ~ScopedViewUpdate(){flag_=previous_;}
    ScopedViewUpdate(const ScopedViewUpdate&)=delete;
    ScopedViewUpdate& operator=(const ScopedViewUpdate&)=delete;
};
}
#endif
