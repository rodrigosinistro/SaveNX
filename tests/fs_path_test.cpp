#include "fs_path.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace nx;
// Reproduce libnx's real fixed-size IPC input consumption across a guard page.
__attribute__((noinline)) static void ipcRead(const char*path)
{
    volatile unsigned char bytes[0x301];for(size_t i=0;i<sizeof bytes;i++)bytes[i]=path[i];
    assert(bytes[0]=='/');
}
int main()
{
    const auto page=sysconf(_SC_PAGESIZE);void*region=mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(region!=MAP_FAILED);
    assert(mprotect(static_cast<char*>(region)+page,page,PROT_NONE)==0);
    auto*shortPath=static_cast<char*>(region)+page-3;std::memcpy(shortPath,"/x",3);
    struct rlimit limit{0,0};setrlimit(RLIMIT_CORE,&limit);
    auto child=fork();assert(child>=0);
    if(child==0){ipcRead(shortPath);_exit(0);}
    int status{};waitpid(child,&status,0);assert(WIFSIGNALED(status));
    FsPath fixed(shortPath);ipcRead(fixed.data());assert(std::string(fixed.data())=="/x");
    for(size_t i=3;i<FsPath::capacity();i++)assert(fixed.data()[i]==0);
    munmap(region,page*2);
    for(auto s:{"/","/folder/data.bin","/folder/subfolder/","/pasta/ação.bin"}){FsPath p(s);ipcRead(p.data());}
    FsPath root("/");assert(std::string(root.data())=="/");FsPath folder("/folder/subfolder/");assert(std::string(folder.data())=="/folder/subfolder");
    FsPath longest("/"+std::string(767,'a'));assert(std::strlen(longest.data())==768);
    for(auto s:{"","relative","/../save","/a//b","/a/./b","/sdmc:/file","/a\\b"}){bool rejected=false;try{FsPath p(s);}catch(...){rejected=true;}assert(rejected);}
    bool rejected=false;try{FsPath p("/"+std::string(768,'a'));}catch(...){rejected=true;}assert(rejected);
    std::cout<<"Filesystem IPC: old short buffer faults across guard page; owned full-size paths, padding, normalization and bounds passed.\n";
}
