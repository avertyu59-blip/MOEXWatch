#pragma once
#ifdef _WIN32
#include <windows.h>
namespace moexwatch::ui::theme {
inline constexpr int sidebar = 184;
inline constexpr int navigation_first = 1100;
inline constexpr int toggle = 1107;
struct Palette { COLORREF canvas, surface, raised, text, muted, accent, selected, border, positive, negative, warning, purple; };
inline constexpr Palette dark{RGB(11,18,25),RGB(20,29,38),RGB(28,40,52),RGB(235,240,245),RGB(172,185,199),RGB(42,137,245),RGB(22,57,92),RGB(65,82,99),RGB(86,201,121),RGB(255,105,117),RGB(242,175,73),RGB(178,124,242)};
inline constexpr Palette light{RGB(243,246,250),RGB(255,255,255),RGB(231,237,244),RGB(24,36,51),RGB(79,96,116),RGB(23,98,192),RGB(214,231,252),RGB(126,143,162),RGB(20,119,65),RGB(188,42,59),RGB(155,92,0),RGB(117,59,176)};
inline Palette colors(bool is_dark) {
    HIGHCONTRASTW hc{sizeof(hc)};
    if(SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0) && (hc.dwFlags&HCF_HIGHCONTRASTON)) {
        auto c=light; c.canvas=c.surface=c.raised=GetSysColor(COLOR_WINDOW);
        c.text=c.muted=c.positive=c.negative=c.warning=c.purple=GetSysColor(COLOR_WINDOWTEXT);
        c.accent=GetSysColor(COLOR_HIGHLIGHT); c.selected=GetSysColor(COLOR_HIGHLIGHT);c.border=c.text;return c;
    }
    return is_dark?dark:light;
}
}
#endif
