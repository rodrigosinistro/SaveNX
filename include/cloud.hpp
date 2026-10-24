#pragma once
#include "platform.hpp"
#include <map>
namespace nx {

class Cloud
{
    std::string client,secret,token,refresh;int64_t expires{};bool linked{};
    void loadClient();void renew(const Context&);void saveSession();
    std::string folder(const std::string&,const std::string&,const Context&,bool create);
    std::string targetFolder(const std::string&,const std::string&,const std::string&,const Context&,bool create);
public:
    bool resume(const Context&);
    bool connected()const{return linked;}
    void connect(const Context&);
    int flush(const Context&);
    std::vector<Backup> list(const Profile&,const Game&,const Context&);
    Backup download(const Backup&,const Context&);
};
}
