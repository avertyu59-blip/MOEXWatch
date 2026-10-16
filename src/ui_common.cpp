#include "ui_app.hpp"
#ifdef _WIN32
namespace moexwatch::ui {
TableSort* Application::table_sort(int id) {
    switch(id) {
    case ID_LIST:return &charts_page.sort;
    case ID_TRADE_LIST:return &trades_page.sort;
    case ID_PORT_POSITIONS:return &portfolio_page.positions_sort;
    case ID_PORT_TRANSACTIONS:return &portfolio_page.transactions_sort;
    case ID_SIG_LIST:return &signals_page.sort;
    case ID_DIVIDENDS_LIST:return &dividends_page.sort;
    default:return nullptr;
    }
}
bool* Application::view_updating(int id) {
    switch(id) {
    case ID_LIST:return &charts_page.updating;
    case ID_TRADE_LIST:return &trades_page.updating;
    case ID_PORT_POSITIONS:case ID_PORT_TRANSACTIONS:return &portfolio_page.updating;
    case ID_SIG_LIST:return &signals_page.updating;
    case ID_DIVIDENDS_LIST:return &dividends_page.updating;
    default:return nullptr;
    }
}
void Application::apply_table_sort(HWND list,int id) {
    const auto* sort=table_sort(id);auto* updating=view_updating(id);
    if(!sort || !updating || sort->column<0)return;
    const int col=sort->column;
    bool numeric=false;
    if(id==ID_LIST)numeric=col>=1&&col<=4;
    else if(id==ID_TRADE_LIST)numeric=col>=1;
    else if(id==ID_PORT_POSITIONS)numeric=col>=1;
    else if(id==ID_PORT_TRANSACTIONS)numeric=col>=3;
    else if(id==ID_SIG_LIST)numeric=col>=1&&col<=4;
    else if(id==ID_DIVIDENDS_LIST)numeric=col==2||col==3;
    const ScopedViewUpdate update(*updating);sort_report(list,*sort,numeric);
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

void Application::add_control(std::vector<HWND>& controls, HWND parent, const wchar_t* type, const wchar_t* caption, DWORD style, int x, int y, int w, int height, int id) {
    HWND control = CreateWindowExW(0, type, caption, WS_CHILD | WS_VISIBLE | style, x, y, w, height, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (shell.font) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(shell.font), TRUE);
    if (control) {
        if (id == ID_REFRESH) shell.global_controls.push_back(control);
        else controls.push_back(control);
    }
}
} // namespace moexwatch::ui
#endif
