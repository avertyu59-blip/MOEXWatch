#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
void Application::apply_table_sort(HWND list,int id) {
    const auto found=app_table_sorts.find(id);if(found==app_table_sorts.end())return;
    const int col=found->second.column;
    bool numeric=false;
    if(id==ID_LIST)numeric=col>=1&&col<=4;
    else if(id==ID_TRADE_LIST)numeric=col>=1;
    else if(id==ID_PORT_POSITIONS)numeric=col>=1;
    else if(id==ID_PORT_TRANSACTIONS)numeric=col>=3;
    else if(id==ID_SIG_LIST)numeric=col>=1&&col<=4;
    else if(id==ID_DIVIDENDS_LIST)numeric=col==2||col==3;
    const bool old=app_filling;app_filling=true;sort_report(list,found->second,numeric);app_filling=old;
}

std::wstring Application::widen(std::string_view text) {
    if (text.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}

std::string Application::narrow(std::wstring_view text) {
    if (text.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring Application::read_edit(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

std::optional<double> Application::parse_finite_number(std::string value) {
    std::replace(value.begin(), value.end(), ',', '.');
    if (value.empty()) return std::nullopt;
    double parsed{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() || !std::isfinite(parsed)) return std::nullopt;
    return parsed;
}

std::string Application::money(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

std::string Application::optional_number(std::optional<double> n) {
    return n ? format_price(*n) : "—";
}

std::string Application::percent(std::optional<double> n) {
    if (!n) return "—";
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << *n << '%';
    return out.str();
}

std::string Application::today_date() {
    SYSTEMTIME now{}; GetLocalTime(&now);
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << now.wYear << '-'
        << std::setw(2) << now.wMonth << '-' << std::setw(2) << now.wDay;
    return out.str();
}

void Application::add_control(HWND parent, const wchar_t* type, const wchar_t* caption, DWORD style, int x, int y, int w, int height, int id) {
    HWND control = CreateWindowExW(0, type, caption, WS_CHILD | WS_VISIBLE | style, x, y, w, height, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (app_font) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(app_font), TRUE);
    if (control) {
        if (id == ID_REFRESH) app_global_controls.push_back(control);
        else if (signals_page.building_signal_page) signals_page.controls.push_back(control);
        else if (backtest_page.building_backtest_page) backtest_page.controls.push_back(control);
        else if (portfolio_page.building_portfolio_page) portfolio_page.controls.push_back(control);
        else if (trades_page.building_trade_page) trades_page.controls.push_back(control);
        else charts_page.market_controls.push_back(control);
    }
}
} // namespace moexwatch::ui
#endif
