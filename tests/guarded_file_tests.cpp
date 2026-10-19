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
    std::filesystem::create_directory(root / "directory.json");
    GuardedFile directory(root / "directory.json");
    rejected([&] { directory.read(); });
    GuardedFile blocked(root / "blocked.json");
    blocked.read();
    blocked.write("original");
    std::filesystem::remove_all(root / "backups/last-good");
    storage::atomic_write(root / "backups/last-good", "not a directory");
    rejected([&] { blocked.write("replacement"); });
    check(*storage::read_text(root / "blocked.json") == "original");
    std::error_code error;
    std::filesystem::create_symlink(path, root / "link.json", error);
    if (!error) {
        GuardedFile link(root / "link.json");
        rejected([&] { link.read(); });
    }
    std::filesystem::remove_all(root);
}
