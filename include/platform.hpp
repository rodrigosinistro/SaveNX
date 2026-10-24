#pragma once
#include "core.hpp"
#include <atomic>
#include <functional>
#include <stdexcept>

namespace nx
{
struct Progress { std::string text,code,url; double value{}; bool canCancel{true}; };
using Report=std::function<void(const Progress&)>;
struct Context { std::atomic_bool* cancel{}; Report report; void check()const { if(cancel&&cancel->load())throw std::runtime_error("Operação cancelada."); } };
void prepare(); void log(const std::string&); void writeText(const std::string&,const std::string&);
std::string readText(const std::string&); std::string timestamp();
std::vector<Profile> profiles(const Context& = {});std::vector<Game> games(const Profile&,const Context&);
std::vector<Backup> localBackups(const Profile&,const Game&);int pendingCount();int backupCount();
std::string gameDirectory(const Profile&,const Game&);
Backup backup(const Profile&,const Game&,const Context&,bool safety=false,bool enqueue=true);
void restore(const Profile&,const Game&,const Backup&,const Context&);
struct Job { std::string id,file,name,user,title,slot; uint64_t size{}; };
std::vector<Job> pending();void acknowledge(const Job&);
}
