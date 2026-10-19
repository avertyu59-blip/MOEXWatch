#pragma once
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
namespace moexwatch {
class FileConflict : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
// Use under the profile lock. Detects external edits/deletion since read();
// preserves the previous bytes before atomic replacement. Not an OS-level CAS.
class GuardedFile {
  public:
    explicit GuardedFile(std::filesystem::path path, std::size_t limit = 4U * 1024U * 1024U);
    std::optional<std::string> read();
    void write(std::string_view bytes);

  private:
    std::filesystem::path path_;
    std::size_t limit_;
    bool loaded_{};
    std::optional<std::string> snapshot_;
    std::optional<std::string> current() const;
};
} // namespace moexwatch
