#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vector>
namespace moexwatch::ui {
struct ShellState {
    HWND window{};
    HWND backup_button{};
    HWND tabs{};
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
