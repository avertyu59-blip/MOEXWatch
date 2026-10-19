#include <winsock2.h>
#include <ws2tcpip.h>
#include "io_runtime.hpp"
#include <future>
#include <iostream>
#include <thread>
#include <atomic>
using namespace moexwatch;
using namespace std::chrono_literals;
namespace {
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
struct Server {
    SOCKET listener{INVALID_SOCKET};unsigned short port{};std::promise<void> received;std::jthread worker;
    explicit Server(int mode){
        listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);check(listener!=INVALID_SOCKET,"socket");
        sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=0;
        check(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0&&listen(listener,1)==0,"bind/listen");
        int length=sizeof(addr);check(getsockname(listener,reinterpret_cast<sockaddr*>(&addr),&length)==0,"port");port=ntohs(addr.sin_port);
        worker=std::jthread([this,mode](std::stop_token stop){
            const auto client=accept(listener,nullptr,nullptr);if(client==INVALID_SOCKET){received.set_value();return;}
            DWORD timeout=2000;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
            char request[4096]{};recv(client,request,sizeof(request),0);
            if(mode!=1){const std::string response=mode==3?"HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:1/untrusted\r\nContent-Length: 0\r\nConnection: close\r\n\r\n":mode==0?"HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello":"HTTP/1.1 200 OK\r\nContent-Length: 10000\r\nConnection: close\r\n\r\nx";send(client,response.data(),static_cast<int>(response.size()),0);}
            received.set_value();
            if(mode==1||mode==2)cancellable_wait(stop,10s);
            shutdown(client,SD_BOTH);closesocket(client);
        });
    }
    ~Server(){worker.request_stop();shutdown(listener,SD_BOTH);closesocket(listener);if(worker.joinable())worker.join();}
};
}
int main(){WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))return 1;try{
    {Server server(0);auto response=winhttp_get({"127.0.0.1","/","loopback",server.port,false},{});check(response.status==200&&response.body=="hello","async HTTP round trip");}
    {Server server(3);auto response=winhttp_get({"127.0.0.1","/","loopback",server.port,false},{});check(response.status==302,"redirect must not be followed");}
    for(int mode:{1,2}){
        Server server(mode);std::atomic<bool> cancelled{};std::exception_ptr failure;
        std::jthread request([&](std::stop_token stop){try{winhttp_get({"127.0.0.1","/","loopback",server.port,false},stop);}catch(const Cancelled&){cancelled=true;}catch(...){failure=std::current_exception();}});
        check(server.received.get_future().wait_for(5s)==std::future_status::ready,"request reached local server");
        // Let WinHTTP reach its pending header/body operation before cancelling it.
        std::this_thread::sleep_for(50ms);const auto start=std::chrono::steady_clock::now();request.request_stop();request.join();
        if(failure)std::rethrow_exception(failure);
        check(cancelled&&std::chrono::steady_clock::now()-start<2s,"pending request cancellation joins promptly");
    }
    std::cout<<"winhttp_tests: async response and cancellation before headers / during body passed\n";
    WSACleanup();return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';WSACleanup();return 1;}}
