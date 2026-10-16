#include "async_tasks.hpp"
namespace moexwatch {
bool CompletionMailbox::publish(BackgroundEvent event){std::lock_guard lock(mutex_);if(!open_)return false;events_.push_back(std::move(event));return true;}
std::vector<BackgroundEvent> CompletionMailbox::take(){std::lock_guard lock(mutex_);std::vector<BackgroundEvent> result;result.swap(events_);return result;}
void CompletionMailbox::close(){std::lock_guard lock(mutex_);open_=false;events_.clear();}
TaskGroup::~TaskGroup(){shutdown();}
bool TaskGroup::start(TaskKind kind,std::shared_ptr<CompletionMailbox> mailbox,Work work) {
    auto& slot=slots_.at(static_cast<std::size_t>(kind));
    if(running(kind))return false;
    if(slot.thread.joinable())slot.thread.join();
    auto active=std::make_shared<std::atomic<bool>>(true);slot.running=active;
    try {
        slot.thread=std::jthread([kind,mailbox=std::move(mailbox),work=std::move(work),active](std::stop_token stop) {
            BackgroundEvent result;
            try{result=work(stop);}
            catch(const Cancelled&){result={0,TaskFailure{kind,true}};}
            catch(...){result={0,TaskFailure{kind,false}};}
            active->store(false,std::memory_order_release);
            mailbox->publish(std::move(result));
        });
    }catch(...){active->store(false);throw;}
    return true;
}
bool TaskGroup::running(TaskKind kind)const{const auto& slot=slots_.at(static_cast<std::size_t>(kind));return slot.running&&slot.running->load(std::memory_order_acquire);}
bool TaskGroup::stop_requested(TaskKind kind)const{return slots_.at(static_cast<std::size_t>(kind)).thread.get_stop_token().stop_requested();}
void TaskGroup::cancel(TaskKind kind){auto& slot=slots_.at(static_cast<std::size_t>(kind));if(slot.thread.joinable())slot.thread.request_stop();}
void TaskGroup::cancel_all(){for(auto& slot:slots_)if(slot.thread.joinable())slot.thread.request_stop();}
void TaskGroup::shutdown(){cancel_all();for(auto& slot:slots_)if(slot.thread.joinable())slot.thread.join();}
}
