#include "portfolio_service.hpp"
#include "storage.hpp"
#include "trading_calendar.hpp"
#include <chrono>
#include <stdexcept>
using namespace moexwatch;
void check(bool x) {
    if (!x)
        throw std::runtime_error("portfolio service test failed");
}
int main() {
    const auto root =
        std::filesystem::temp_directory_path() /
        ("moex-service-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    AppStorage storage(root);
    PortfolioService service(storage);
    service.load();
    check(service.readable());
    service.commit(service.with_operation({"2026-01-01", "SBER", PortfolioSide::Buy, 10, 100, 2}));
    const auto snapshot = service.journal();
    check(snapshot.transactions.size() == 1);
    const auto revision = service.revision();
    PortfolioService restarted(storage);
    restarted.load();
    check(restarted.journal().transactions.front().id == snapshot.transactions.front().id);
    service.commit(service.with_operation({"2026-01-02", "SBER", PortfolioSide::Sell, 2, 110, 1}));
    bool rejected = false;
    try {
        service.import_transactions(snapshot.transactions, revision);
    } catch (...) {
        rejected = true;
    }
    check(rejected && service.journal().transactions.size() == 2);
    check(storage::read_text(root / "backups/last-good/portfolio_journal_v2.csv").has_value());
    const auto summary = service.calculate({});
    check(summary && summary->positions.front().quantity == 8);
    storage::atomic_write(root / "portfolio_journal_v2.csv", "external");
    rejected = false;
    try {
        service.commit(service.journal());
    } catch (const FileConflict &) {
        rejected = true;
    }
    check(rejected && *storage::read_text(root / "portfolio_journal_v2.csv") == "external");
    check(!trading_calendar_for("2026-10-01").empty());
    check(trading_calendar_for("2027-01-01").empty());
    check(trading_calendar_for("2026-02-30").empty());
    std::filesystem::remove_all(root);
}
