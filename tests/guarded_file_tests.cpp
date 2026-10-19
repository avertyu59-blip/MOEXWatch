#include "guarded_file.hpp"
#include "storage.hpp"
#include <chrono>
#include <stdexcept>
using namespace moexwatch;
void check(bool x) {
    if (!x)
        throw std::runtime_error("guarded file check failed");
}
template <class F> void rejected(F f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception &) {
        caught = true;
    }
    check(caught);
}
int main() {
    const auto root =
        std::filesystem::temp_directory_path() /
        ("moex-guard-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto path = root / "test.json";
    GuardedFile f(path, 100);
    rejected([&] { f.write("a"); });
    check(!f.read());
    f.write("a");
    f.write("b");
    check(storage::read_text(root / "backups/last-good/test.json") == std::optional<std::string>("a"));
    storage::atomic_write(path, "external");
    rejected([&] { f.write("c"); });
    check(*storage::read_text(path) == "external");
    check(*f.read() == "external");
    std::filesystem::remove(path);
    rejected([&] { f.write("c"); });
    check(!storage::read_text(path));
    GuardedFile absent(path);
    check(!absent.read());
    storage::atomic_write(path, "new");
    rejected([&] { absent.write("overwrite"); });
    f.read();
    rejected([&] { f.write(std::string(101, 'x')); });
    check(*storage::read_text(path) == "new");
    std::filesystem::remove_all(root);
}
