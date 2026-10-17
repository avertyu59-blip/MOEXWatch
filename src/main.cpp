#include "ui_app.hpp"
#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    moexwatch::ui::Application app;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES}; InitCommonControlsEx(&controls);
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = moexwatch::ui::Application::window_proc; wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MOEXWatchWindow"; wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return 1;
    WNDCLASSEXW dividend_wc{}; dividend_wc.cbSize = sizeof(dividend_wc);
    dividend_wc.lpfnWndProc = moexwatch::ui::Application::dividends_window_proc; dividend_wc.hInstance = instance;
    dividend_wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    dividend_wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    dividend_wc.lpszClassName = L"MOEXWatchDividendWindow";
    if (!RegisterClassExW(&dividend_wc)) return 1;
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"MOEX Watch 0.6.8 — аналитика и портфель", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1500, 1000, nullptr, nullptr, instance, &app);
    if (!window) return 2;
    ShowWindow(window, show); UpdateWindow(window);
    MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { if(!IsDialogMessageW(window,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    return static_cast<int>(msg.wParam);
}

#else
#include "market_core.hpp"
#include <iostream>
int main() {
    using namespace moexwatch;
    const std::string fixture = "begin;open;high;low;close;volume\n2026-10-01;100;103;99;102;1000\n2026-10-02;102;104;101;103;1200\n";
    const auto candles = parse_candles_csv(fixture);
    const auto result = analyze(candles);
    std::cout << "MOEXWatch portable core check\n"
              << "Parsed candles: " << candles.size() << "\n"
              << "Change: " << (result.period_change_percent ? format_price(*result.period_change_percent) + "%" : "n/a") << "\n"
              << "For the Windows 10 GUI build use build_windows.ps1.\n";
}
#endif
