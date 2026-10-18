#include "profile_lock.hpp"
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <mutex>
#include <set>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace moexwatch {
#ifdef _WIN32
namespace {
std::mutex registryMutex;
std::set<std::wstring> occupied;
std::wstring profileKey(const std::filesystem::path& root) {
    auto path=std::filesystem::weakly_canonical(std::filesystem::absolute(root)).lexically_normal().wstring();
    while(path.size()>3&&(path.back()==L'\\'||path.back()==L'/'))path.pop_back();
    if(!path.empty()) {
        std::wstring lower(path.size(),L'\0');
        if(!LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,path.data(),static_cast<int>(path.size()),lower.data(),static_cast<int>(lower.size()),nullptr,nullptr,0))
            throw std::runtime_error("Cannot normalize profile path");
        path=std::move(lower);
    }
    // Stable across compilers, binaries and process launches; std::hash is not.
    std::uint64_t hash=14695981039346656037ULL;
    for(const auto ch:path){hash^=static_cast<unsigned>(ch)&255U;hash*=1099511628211ULL;hash^=(static_cast<unsigned>(ch)>>8)&255U;hash*=1099511628211ULL;}
    return L"Local\\MOEXWatch.Profile."+std::to_wstring(hash);
}
}
#endif
ProfileLock::ProfileLock(std::filesystem::path root) {
    if(root.empty())throw std::invalid_argument("Profile lock requires a directory");
#ifdef _WIN32
    key_=profileKey(root);
    std::lock_guard guard(registryMutex);
    // Windows mutexes are recursive for one thread, so also exclude a second
    // controller in this process before attempting the kernel mutex.
    if(occupied.contains(key_))return;
    HANDLE handle=CreateMutexW(nullptr,FALSE,key_.c_str());
    if(!handle)throw std::runtime_error("Cannot create profile mutex");
    const auto result=WaitForSingleObject(handle,0);
    if(result==WAIT_TIMEOUT){CloseHandle(handle);return;}
    if(result!=WAIT_OBJECT_0&&result!=WAIT_ABANDONED){CloseHandle(handle);throw std::runtime_error("Cannot acquire profile mutex");}
    try{occupied.insert(key_);}catch(...){ReleaseMutex(handle);CloseHandle(handle);throw;}
    native_=reinterpret_cast<std::intptr_t>(handle);owned_=true;
#else
    const auto file=std::filesystem::weakly_canonical(std::filesystem::absolute(root))/"profile-session.lock";
    const int fd=::open(file.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(fd<0)throw std::runtime_error("Cannot open profile lock file");
    if(::flock(fd,LOCK_EX|LOCK_NB)!=0){const int reason=errno;::close(fd);if(reason==EWOULDBLOCK||reason==EAGAIN)return;throw std::runtime_error("Cannot acquire profile lock");}
    native_=fd;owned_=true;
#endif
}
ProfileLock::~ProfileLock() {
    if(!owned_)return;
#ifdef _WIN32
    std::lock_guard guard(registryMutex);
    ReleaseMutex(reinterpret_cast<HANDLE>(native_));CloseHandle(reinterpret_cast<HANDLE>(native_));occupied.erase(key_);
#else
    ::flock(static_cast<int>(native_),LOCK_UN);::close(static_cast<int>(native_));
#endif
}
}
