#pragma once
#include "app_models.hpp"
#include "io_runtime.hpp"
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <stop_token>
#include <thread>
#include <variant>
namespace moexwatch {
enum class TaskKind:std::size_t {Market,Dividends,Backtest,Backup,Count};
struct BacktestResult {std::string text;bool failed{};bool cancelled{};};
struct BackupResult {std::filesystem::path path;std::string error;bool cancelled{};};
struct FetchProgress {std::size_t current{},total{};};
struct TaskFailure {TaskKind kind;bool cancelled{};};
struct BackgroundEvent {
    std::uint64_t request{};
    std::variant<MarketRefreshResult,DividendFetchResult,BacktestResult,BackupResult,FetchProgress,TaskFailure> data;
};
class CompletionMailbox {
    std::mutex mutex_;bool open_{true};std::vector<BackgroundEvent> events_;
public:
    bool publish(BackgroundEvent event);
    std::vector<BackgroundEvent> take();
    void close();
};
// UI-thread owner. Only the worker's running flag and mailbox are shared.
class TaskGroup {
    struct Slot {std::jthread thread;std::shared_ptr<std::atomic<bool>> running;};
    std::array<Slot,static_cast<std::size_t>(TaskKind::Count)> slots_;
public:
    using Work=std::function<BackgroundEvent(std::stop_token)>;
    ~TaskGroup();
    bool start(TaskKind kind,std::shared_ptr<CompletionMailbox> mailbox,Work work);
    bool running(TaskKind kind) const;
    bool stop_requested(TaskKind kind) const;
    void cancel(TaskKind kind);
    void cancel_all();
    void shutdown();
};
}
