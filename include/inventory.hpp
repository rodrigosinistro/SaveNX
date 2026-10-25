#pragma once
#include "platform.hpp"
#include <set>
#include <tuple>
namespace nx {
// These are libnx User/SdUser and Account/Device values, checked against the SDK by discovery.cpp.
enum class QueryMode { Typed, Space, All };
struct SaveQuery { QueryMode mode;int space{1},type{1},rank{}; };
struct Inventory { std::vector<Game> saves;int successfulReaders{},readRecords{},unresolved{};std::vector<std::string> errors; };
using SaveVisitor=std::function<void(Game)>;
using SaveReader=std::function<void(const SaveQuery&,const SaveVisitor&)>;
inline Inventory scanInventory(const SaveReader&read,const Context&ctx)
{
    Inventory out;std::set<std::pair<int,uint64_t>> seen;
    auto visit=[&](Game g){
        ctx.check();out.readRecords++;
        if((g.type!=1&&g.type!=3)||(g.space!=1&&g.space!=4))return;
        if(!g.title||!g.saveId||(g.type==1&&g.uid==std::array<uint64_t,2>{})){out.unresolved++;return;}
        if(seen.insert({g.space,g.saveId}).second)out.saves.push_back(std::move(g));
    };
    auto attempt=[&](SaveQuery q){
        ctx.check();try{read(q,visit);out.successfulReaders++;}catch(const std::exception&e){ctx.check();out.errors.push_back(e.what());}
    };
    // Internal filtered readers and the ordinary reader have different FS access checks.
    // Try both and merge by actual save ID, without assuming a profile or an installed title.
    for(int space:{1,4}){
        for(int type:{1,3})for(int rank:{0,1})attempt({QueryMode::Typed,space,type,rank});
        attempt({QueryMode::Space,space});
    }
    if(out.saves.empty())attempt({QueryMode::All,-1});
    if(!out.successfulReaders)throw std::runtime_error("O console não permitiu consultar os saves. Abra o Homebrew Menu com memória completa.");
    return out;
}
inline std::vector<Game> selectSaves(const Inventory&i,const Profile&p)
{
    std::vector<Game> out;for(auto&g:i.saves)if(owns(p,g))out.push_back(g);return out;
}
}
