#include "io_runtime.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <array>
#include <algorithm>
#include <condition_variable>
#include <memory>
namespace moexwatch {
namespace {
struct Handle {
    HINTERNET value{};
    explicit Handle(HINTERNET h):value(h){}
    ~Handle(){if(value)WinHttpCloseHandle(value);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
struct AsyncState {
    std::mutex mutex;std::condition_variable_any cv;
    DWORD completed{},error{},bytes{};bool closed{};
    std::array<char,16384> buffer{};
};
void CALLBACK callback(HINTERNET,DWORD_PTR context,DWORD status,LPVOID info,DWORD length) {
    if(!context)return;
    auto* binding=reinterpret_cast<std::shared_ptr<AsyncState>*>(context);
    const auto state=*binding; // Keep storage/read buffer alive until this callback returns.
    {
        std::lock_guard lock(state->mutex);
        if(status==WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)state->closed=true;
        else if(status==WINHTTP_CALLBACK_STATUS_REQUEST_ERROR){state->error=static_cast<WINHTTP_ASYNC_RESULT*>(info)->dwError;state->completed=status;}
        else if(status==WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE||status==WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE||
                status==WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE||status==WINHTTP_CALLBACK_STATUS_READ_COMPLETE) {
            state->completed=status;
            if(status==WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE)state->bytes=*static_cast<DWORD*>(info);
            if(status==WINHTTP_CALLBACK_STATUS_READ_COMPLETE)state->bytes=length;
        }
        state->cv.notify_all();
    }
    // HANDLE_CLOSING is the final WinHTTP notification for this request.
    if(status==WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)delete binding;
}
bool retry_error(DWORD error) {
    return error==ERROR_WINHTTP_TIMEOUT||error==ERROR_WINHTTP_CANNOT_CONNECT||error==ERROR_WINHTTP_CONNECTION_ERROR||
        error==ERROR_WINHTTP_NAME_NOT_RESOLVED||error==ERROR_WINHTTP_RESEND_REQUEST;
}
[[noreturn]] void fail(DWORD error){throw NetworkFailure("Ошибка WinHTTP "+std::to_string(error),retry_error(error),error);}
std::wstring wide(std::string_view text){return std::wstring(text.begin(),text.end());}
struct Request {
    HINTERNET handle{};std::shared_ptr<AsyncState> state=std::make_shared<AsyncState>();bool subscribed{};
    ~Request(){
        if(handle){WinHttpCloseHandle(handle);handle=nullptr;}
        if(subscribed){std::unique_lock lock(state->mutex);state->cv.wait(lock,[&]{return state->closed;});}
    }
    void prepare(){std::lock_guard lock(state->mutex);state->completed=0;state->error=0;state->bytes=0;}
    void wait(DWORD expected,std::stop_token stop,std::chrono::steady_clock::time_point deadline) {
        std::unique_lock lock(state->mutex);
        const bool completed=state->cv.wait_until(lock,stop,deadline,[&]{return state->completed!=0;});
        check_cancelled(stop);if(!completed)fail(ERROR_WINHTTP_TIMEOUT);
        if(state->error)fail(state->error);
        if(state->completed!=expected)throw NetworkFailure("Неверное состояние WinHTTP",false);
    }
};
void issued(BOOL value){if(!value){const auto code=GetLastError();if(code!=ERROR_IO_PENDING)fail(code);}}
}
HttpResponse winhttp_get(const HttpRequest& input,std::stop_token stop) {
    check_cancelled(stop);
    if(input.host.empty()||input.path.empty()||input.path[0]!='/'||(!input.secure&&input.host!="127.0.0.1"))
        throw ValidationFailure("Недопустимый адрес источника");
    Handle session(WinHttpOpen(L"MOEXWatch/0.6.7",input.host=="127.0.0.1"?WINHTTP_ACCESS_TYPE_NO_PROXY:WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,WINHTTP_FLAG_ASYNC));
    if(!session.value)fail(GetLastError());
    if(!WinHttpSetTimeouts(session.value,5000,5000,6000,6000))fail(GetLastError());
    const auto host=wide(input.host),path=wide(input.path);
    Handle connection(WinHttpConnect(session.value,host.c_str(),input.port,0));if(!connection.value)fail(GetLastError());
    Request request;request.handle=WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,input.secure?WINHTTP_FLAG_SECURE:0);
    if(!request.handle)fail(GetLastError());
    auto binding=std::make_unique<std::shared_ptr<AsyncState>>(request.state);
    auto context=reinterpret_cast<DWORD_PTR>(binding.get());
    if(!WinHttpSetOption(request.handle,WINHTTP_OPTION_CONTEXT_VALUE,&context,sizeof(context)))fail(GetLastError());
    if(WinHttpSetStatusCallback(request.handle,callback,WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS|WINHTTP_CALLBACK_FLAG_HANDLES,0)==WINHTTP_INVALID_STATUS_CALLBACK)fail(GetLastError());
    request.subscribed=true;binding.release();
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if(!WinHttpSetOption(request.handle,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect)))fail(GetLastError());
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds{20};
    request.prepare();issued(WinHttpSendRequest(request.handle,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,context));
    request.wait(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,stop,deadline);
    request.prepare();issued(WinHttpReceiveResponse(request.handle,nullptr));request.wait(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,stop,deadline);
    HttpResponse result;DWORD size=sizeof(DWORD),status=0;
    if(!WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))fail(GetLastError());
    result.status=status;
    for(;;){
        check_cancelled(stop);if(std::chrono::steady_clock::now()>=deadline)fail(ERROR_WINHTTP_TIMEOUT);request.prepare();issued(WinHttpQueryDataAvailable(request.handle,nullptr));request.wait(WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE,stop,deadline);
        const auto count=request.state->bytes;if(!count)break;
        const auto read=static_cast<DWORD>(std::min<std::size_t>(count,request.state->buffer.size()));
        request.prepare();issued(WinHttpReadData(request.handle,request.state->buffer.data(),read,nullptr));request.wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE,stop,deadline);
        const auto received=request.state->bytes;if(!received)break;
        if(result.body.size()+received>2U*1024U*1024U)throw ValidationFailure("Ответ источника превышает 2 МБ");
        result.body.append(request.state->buffer.data(),received);
    }
    check_cancelled(stop);return result;
}
}
#endif
