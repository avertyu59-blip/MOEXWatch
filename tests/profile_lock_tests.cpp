#include "profile_lock.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#endif
using moexwatch::ProfileLock;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int child(const std::filesystem::path& executable,const std::filesystem::path& root,bool expected) {
#ifdef _WIN32
    std::wstring command=L"\""+executable.wstring()+L"\" --probe \""+root.wstring()+L"\" "+(expected?L"1":L"0");
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    check(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)!=0,"launch lock probe");
    CloseHandle(process.hThread);
    if(WaitForSingleObject(process.hProcess,5000)!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,99);CloseHandle(process.hProcess);throw std::runtime_error("lock probe timed out");}
    DWORD code{};GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);return static_cast<int>(code);
#else
    const auto pid=fork();check(pid>=0,"fork lock probe");
    if(pid==0){execl(executable.c_str(),executable.c_str(),"--probe",root.c_str(),expected?"1":"0",static_cast<char*>(nullptr));_exit(98);}
    int status{};const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for(;;){const auto done=waitpid(pid,&status,WNOHANG);check(done>=0,"wait lock probe");if(done==pid)break;
        if(std::chrono::steady_clock::now()>=deadline){kill(pid,SIGKILL);waitpid(pid,&status,0);throw std::runtime_error("lock probe timed out");}
        std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    return WIFEXITED(status)?WEXITSTATUS(status):97;
#endif
}
int run(const std::filesystem::path& executable,const std::filesystem::path& probeRoot,bool probe,bool expected) {
    if(probe){ProfileLock lock(probeRoot);return lock.ownsLock()==expected?0:6;}
    const auto root=std::filesystem::temp_directory_path()/("moex-profile-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    try {
        {
            ProfileLock first(root);check(first.ownsLock(),"first owner accepted");
            ProfileLock second(root/".");check(!second.ownsLock(),"same-process duplicate rejected including normalized alias");
            check(child(executable,root,false)==0,"separate process cannot own active profile");
            const auto other=root/"other";std::filesystem::create_directory(other);ProfileLock distinct(other);check(distinct.ownsLock(),"different profiles remain independent");
        }
        check(child(executable,root,true)==0,"separate process can own released profile");
        ProfileLock reacquired(root);check(reacquired.ownsLock(),"owner reacquired after probe exits");
    } catch(...) {std::filesystem::remove_all(root);throw;}
    std::filesystem::remove_all(root);
    std::cout<<"profile_lock_tests: same-process, separate-process, normalized paths and release/reacquire passed\n";return 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    try{return run(std::filesystem::absolute(argv[0]),argc>2?std::filesystem::path(argv[2]):std::filesystem::path{},argc==4&&std::wstring(argv[1])==L"--probe",argc==4&&std::wstring(argv[3])==L"1");}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
#else
int main(int argc,char** argv) {
    try{return run(std::filesystem::absolute(argv[0]),argc>2?std::filesystem::path(argv[2]):std::filesystem::path{},argc==4&&std::string(argv[1])=="--probe",argc==4&&std::string(argv[3])=="1");}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
#endif
