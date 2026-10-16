#include "app_storage.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
struct TempDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() / "moexwatch-app-storage-test";
    TempDirectory() { std::filesystem::remove_all(path); std::filesystem::create_directories(path); }
    ~TempDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
}

int main() {
    try {
        TempDirectory temp;
        moexwatch::AppStorage storage(temp.path);

        moexwatch::SignalSettings settings;
        settings.rsi_oversold_below = 24.0;
        storage.save_signal_settings(settings);
        moexwatch::SignalSettings restored_settings;
        restored_settings.rsi_oversold_below = 28.0;
        restored_settings = storage.load_signal_settings(restored_settings);
        check(restored_settings.rsi_oversold_below == 24.0, "signal settings survive AppStorage restart");

        moexwatch::Trade trade{"SBER", 2.0, 100.0, 120.0, 3.0, 13.0};
        storage.save_trades({trade});
        check(storage.load_trades().size() == 1, "trades round-trip through AppStorage");

        moexwatch::Instrument instrument;
        instrument.ticker = "SBER";
        instrument.quote.ticker = "SBER";
        instrument.target = 250.0;
        storage.save_watchlist({instrument});
        const auto watchlist = storage.load_watchlist();
        check(watchlist.size() == 1 && watchlist.front().target == 250.0, "watchlist round-trip through AppStorage");

        const auto corrupt = *storage.read_text("signals.ini");
        storage.atomic_write("signals.ini", corrupt + "x");
        auto retained = settings;
        bool rejected = false;
        try { retained = storage.load_signal_settings(retained); }
        catch (const std::exception&) { rejected = true; }
        check(rejected && retained.rsi_oversold_below == 24.0, "failed load rejects corruption without applying partial settings");

        const auto journal = storage.load_portfolio();
        check(journal.transactions.empty() && journal.levels.empty(), "missing portfolio is an empty typed journal");
        std::cout << "All AppStorage tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
