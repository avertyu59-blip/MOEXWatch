#include "guarded_file.hpp"
#include "storage.hpp"
#include <stdexcept>
namespace moexwatch {
namespace {
void regularOrAbsent(const std::filesystem::path &path) {
    std::error_code ec;
    const auto s = std::filesystem::symlink_status(path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory)
        throw std::runtime_error("Не удалось проверить файл");
    if (std::filesystem::is_symlink(s) ||
        (std::filesystem::exists(s) && !std::filesystem::is_regular_file(s)))
        throw std::runtime_error("Ожидался обычный файл, ссылки и каталоги не допускаются");
}
} // namespace
GuardedFile::GuardedFile(std::filesystem::path path, std::size_t limit)
    : path_(std::move(path)), limit_(limit) {}
std::optional<std::string> GuardedFile::current() const {
    regularOrAbsent(path_);
    return storage::read_text(path_, limit_);
}
std::optional<std::string> GuardedFile::read() {
    auto bytes = current();
    snapshot_ = bytes;
    loaded_ = true;
    return bytes;
}
void GuardedFile::write(std::string_view bytes) {
    if (!loaded_)
        throw std::runtime_error("Файл не был проверен перед сохранением");
    if (bytes.size() > limit_)
        throw std::runtime_error("Превышен размер файла");
    if (current() != snapshot_)
        throw FileConflict(
            "Файл изменён извне; перезапустите приложение для проверки. Изменение не записано");
    if (snapshot_ && *snapshot_ == bytes)
        return;
    if (snapshot_) {
        const auto base = path_.parent_path() / "backups";
        const auto directory = base / "last-good";
        for (const auto &dir : {base, directory}) {
            std::error_code ec;
            const auto s = std::filesystem::symlink_status(dir, ec);
            if (ec && ec != std::errc::no_such_file_or_directory)
                throw std::runtime_error("Недоступен каталог резервной копии");
            if (std::filesystem::is_symlink(s) ||
                (std::filesystem::exists(s) && !std::filesystem::is_directory(s)))
                throw std::runtime_error("Недопустимый каталог резервной копии");
        }
        const auto backup = directory / path_.filename();
        regularOrAbsent(backup);
        storage::atomic_write(backup, *snapshot_);
        if (storage::read_text(backup, limit_) != snapshot_)
            throw std::runtime_error("Не пройдена проверка резервной копии");
    }
    if (current() != snapshot_)
        throw FileConflict("Файл изменён во время сохранения; исходник не заменён");
    storage::atomic_write(path_, bytes);
    snapshot_ = std::string(bytes);
}
} // namespace moexwatch
