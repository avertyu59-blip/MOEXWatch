#include "ui_app.hpp"
#include "profile_lock.hpp"
#include "storage.hpp"
#include <memory>
#include <exception>
#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    std::unique_ptr<moexwatch::ProfileLock> profile;
    try {
        profile=std::make_unique<moexwatch::ProfileLock>(moexwatch::storage::data_directory());
        if(!profile->ownsLock()) {
            MessageBoxW(nullptr,L"Этот профиль уже открыт в другой копии MOEXWatch (Qt или Classic). Закройте её перед запуском.",L"MOEXWatch — профиль занят",MB_OK|MB_ICONINFORMATION);
            return 3;
        }
    } catch(const std::exception&) {
        MessageBoxW(nullptr,L"Не удалось защитить каталог данных. Приложение закрыто, чтобы избежать одновременной записи.",L"MOEXWatch — ошибка профиля",MB_OK|MB_ICONERROR);
        return 4;
    }
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
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"MOEX Watch 0.7.2 — аналитика и портфель", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1340, 870, nullptr, nullptr, instance, &app);
    if (!window) return 2;
    ShowWindow(window, show); UpdateWindow(window);
    MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
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
