#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <stop_token>
#include <stdexcept>
#include <string>
namespace moexwatch {
struct Cancelled:std::runtime_error {Cancelled():std::runtime_error("Загрузка отменена") {}};
struct NetworkFailure:std::runtime_error {
    bool retryable;unsigned code;
    NetworkFailure(std::string text,bool retry,unsigned value=0):std::runtime_error(std::move(text)),retryable(retry),code(value){}
};
struct ValidationFailure:std::runtime_error {using std::runtime_error::runtime_error;};
void check_cancelled(std::stop_token stop);
bool cancellable_wait(std::stop_token stop,std::chrono::milliseconds duration);
struct HttpRequest {
    std::string host;
    std::string path;
    std::string source;
    unsigned short port{443};
    bool secure{true};
};
struct HttpResponse {unsigned status{};std::string body;};
using HttpTransport=std::function<HttpResponse(const HttpRequest&,std::stop_token)>;
class DiagnosticLog {
    std::filesystem::path path_;
    std::mutex mutex_;
    std::size_t limit_;
public:
    explicit DiagnosticLog(std::filesystem::path path,std::size_t limit=256U*1024U):path_(std::move(path)),limit_(limit){}
    const std::filesystem::path& path() const {return path_;}
    void record(std::uint64_t request,std::string_view source,std::string_view stage,
        unsigned attempt=0,unsigned code=0,long long elapsed_ms=0) noexcept;
};
struct RetryPolicy {unsigned attempts{3};std::chrono::milliseconds delay{1000};};
HttpResponse get_with_retry(const HttpRequest& request,const HttpTransport& transport,std::stop_token stop,
    DiagnosticLog* log=nullptr,std::uint64_t request_id=0,RetryPolicy policy={});
#ifdef _WIN32
HttpResponse winhttp_get(const HttpRequest& request,std::stop_token stop);
#endif
}
