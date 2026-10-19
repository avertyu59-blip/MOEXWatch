#include "io_runtime.hpp"
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace moexwatch {
void check_cancelled(std::stop_token stop){if(stop.stop_requested())throw Cancelled();}
bool cancellable_wait(std::stop_token stop,std::chrono::milliseconds duration) {
    std::mutex mutex;std::condition_variable_any cv;std::unique_lock lock(mutex);
    cv.wait_for(lock,stop,duration,[]{return false;});return !stop.stop_requested();
}
namespace {
std::string safe(std::string_view value){std::string result;for(const char c:value){if(result.size()==64)break;if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-')result+=c;}return result;}
bool retry_status(unsigned status){return status==408||status==429||status==500||status==502||status==503||status==504;}
bool regular_or_absent(const std::filesystem::path& path) {
    std::error_code error;
    const auto status=std::filesystem::symlink_status(path,error);
    return (!error||error==std::errc::no_such_file_or_directory)&&
        !std::filesystem::is_symlink(status)&&
        (!std::filesystem::exists(status)||std::filesystem::is_regular_file(status));
}
}
void DiagnosticLog::record(std::uint64_t request,std::string_view source,std::string_view stage,unsigned attempt,unsigned code,long long elapsed) noexcept {
    try {
        if(path_.empty())return;
        std::lock_guard lock(mutex_);std::filesystem::create_directories(path_.parent_path());
        auto one=path_;one+=".1";auto two=path_;two+=".2";
        if(!regular_or_absent(path_)||!regular_or_absent(one)||!regular_or_absent(two))return;
        std::ostringstream line;
        const auto now=std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        const auto date=std::chrono::floor<std::chrono::days>(now);const std::chrono::year_month_day ymd{date};
        const auto seconds=(now-date).count();
        line<<int(ymd.year())<<'-'<<std::setfill('0')<<std::setw(2)<<unsigned(ymd.month())<<'-'<<std::setw(2)<<unsigned(ymd.day())
            <<'T'<<std::setw(2)<<seconds/3600<<':'<<std::setw(2)<<(seconds/60)%60<<':'<<std::setw(2)<<seconds%60<<"Z request="<<request
            <<" source="<<safe(source)<<" stage="<<safe(stage)<<" attempt="<<attempt<<" code="<<code<<" ms="<<elapsed<<'\n';
        const auto text=line.str();
        if(std::filesystem::exists(path_)&&std::filesystem::file_size(path_)+text.size()>limit_) {
            std::error_code ignored;std::filesystem::remove(two,ignored);
            if(std::filesystem::exists(one))std::filesystem::rename(one,two);
            std::filesystem::rename(path_,one);
        }
        std::ofstream out(path_,std::ios::binary|std::ios::app);out<<text;
    }catch(...){/* Diagnostics must never replace the original data/error path. */}
}
HttpResponse get_with_retry(const HttpRequest& request,const HttpTransport& transport,std::stop_token stop,
    DiagnosticLog* log,std::uint64_t id,RetryPolicy policy) {
    if(!transport||policy.attempts<1||policy.attempts>3||policy.delay.count()<0||policy.delay>std::chrono::seconds{10})
        throw std::invalid_argument("Некорректная политика загрузки");
    for(unsigned attempt=1;attempt<=policy.attempts;++attempt) {
        check_cancelled(stop);const auto start=std::chrono::steady_clock::now();
        if(log)log->record(id,request.source,"receive_begin",attempt);
        try {
            auto response=transport(request,stop);check_cancelled(stop);
            if(response.body.size()>2U*1024U*1024U)throw ValidationFailure("Ответ источника превышает 2 МБ");
            if(response.status<200||response.status>=300)
                throw NetworkFailure("Источник ответил HTTP "+std::to_string(response.status),retry_status(response.status),response.status);
            if(log)log->record(id,request.source,"receive_ok",attempt,response.status,std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count());
            return response;
        } catch(const Cancelled&){if(log)log->record(id,request.source,"cancelled",attempt);throw;}
        catch(const NetworkFailure& ex) {
            if(log)log->record(id,request.source,"receive_error",attempt,ex.code,std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count());
            check_cancelled(stop);
            if(!ex.retryable||attempt==policy.attempts)throw;
            if(log)log->record(id,request.source,"retry_wait",attempt,ex.code);
            if(!cancellable_wait(stop,policy.delay*attempt))throw Cancelled();
        } catch(...){if(log)log->record(id,request.source,"receive_rejected",attempt);throw;}
    }
    throw NetworkFailure("Попытки исчерпаны",false);
}
}
