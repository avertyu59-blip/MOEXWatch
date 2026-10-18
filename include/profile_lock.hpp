#pragma once
#include <filesystem>
#include <cstdint>
#include <string>
namespace moexwatch {
// One writer process per profile, shared by the Qt and Classic executables.
// Construct before reading/migrating profile files. Nonblocking; failures throw,
// an already occupied profile is represented by ownsLock() == false.
class ProfileLock final {
public:
    explicit ProfileLock(std::filesystem::path root);
    ~ProfileLock();
    ProfileLock(const ProfileLock&)=delete;
    ProfileLock& operator=(const ProfileLock&)=delete;
    bool ownsLock() const noexcept { return owned_; }
private:
    std::intptr_t native_{-1};
    bool owned_{};
    std::wstring key_;
};
}
