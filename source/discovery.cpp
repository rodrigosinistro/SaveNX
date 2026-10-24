#include "inventory.hpp"
#include "json_util.hpp"
#include <switch.h>
#include <map>
#include <algorithm>
#include <memory>

namespace nx {
static_assert(FsSaveDataSpaceId_User==1&&FsSaveDataSpaceId_SdUser==4);
static_assert(FsSaveDataType_Account==1&&FsSaveDataType_Device==3);
static std::string queryName(const SaveQuery&q)
{
    return q.mode==QueryMode::All?"all":std::string(q.mode==QueryMode::Typed?"filtered":"space")+" space="+std::to_string(q.space)+" type="+std::to_string(q.type)+" rank="+std::to_string(q.rank);
}
static void check(Result r,const std::string&what)
{
    if(R_FAILED(r)){auto message=what+" code="+hex(r);log(message);throw std::runtime_error(message);}
}
static void readNative(const SaveQuery&q,const SaveVisitor&visit)
{
    FsSaveDataInfoReader reader{};FsSaveDataFilter filter{};
    filter.filter_by_save_data_type=true;filter.attr.save_data_type=q.type;filter.save_data_rank=q.rank;filter.attr.save_data_rank=q.rank;
    auto rc=q.mode==QueryMode::Typed?fsOpenSaveDataInfoReaderWithFilter(&reader,static_cast<FsSaveDataSpaceId>(q.space),&filter):fsOpenSaveDataInfoReader(&reader,static_cast<FsSaveDataSpaceId>(q.space));
    auto label=queryName(q);check(rc,"Open save reader "+label);int readTotal=0;
    try{
        while(true){
            FsSaveDataInfo page[16]{};s64 count{};
            check(fsSaveDataInfoReaderRead(&reader,page,16,&count),"Read saves "+label);
            if(count<0||count>16)throw std::runtime_error("Invalid save reader count");if(!count)break;
            for(int i=0;i<count;i++){
                auto&s=page[i];readTotal++;
                Game g{s.application_id,s.save_data_id,{s.uid.uid[0],s.uid.uid[1]},s.save_data_type,s.save_data_rank,s.save_data_index,{}};
                g.space=q.mode==QueryMode::All?s.save_data_space_id:q.space;
                if((g.space==FsSaveDataSpaceId_User||g.space==FsSaveDataSpaceId_SdUser)&&(g.type==FsSaveDataType_Account||g.type==FsSaveDataType_Device)&&(!g.title||(g.type==FsSaveDataType_Account&&g.uid==std::array<uint64_t,2>{}))){
                    FsSaveDataExtraData extra{};
                    auto result=fsReadSaveDataFileSystemExtraDataBySaveDataSpaceId(&extra,sizeof extra,static_cast<FsSaveDataSpaceId>(g.space),g.saveId);
                    if(R_SUCCEEDED(result)){
                        if(!g.title)g.title=extra.attr.application_id?extra.attr.application_id:extra.owner_id;
                        if(g.type==FsSaveDataType_Account&&g.uid==std::array<uint64_t,2>{})g.uid={extra.attr.uid.uid[0],extra.attr.uid.uid[1]};
                    }else log("Save identity lookup failed code="+hex(result));
                }
                if(readTotal<=256)log("Save record space="+std::to_string(g.space)+" type="+std::to_string(g.type)+" rank="+std::to_string(g.rank)+" title="+hex(g.title)+" save="+hex(g.saveId));
                visit(std::move(g));
            }
        }
        fsSaveDataInfoReaderClose(&reader);log("Save scan "+label+" records="+std::to_string(readTotal));
    }catch(...){fsSaveDataInfoReaderClose(&reader);throw;}
}
static Inventory inventory(const Context&ctx)
{
    if(ctx.report)ctx.report({"Buscando os saves do console...",{},{},0.1,true});
    Inventory found;
    try{found=scanInventory(readNative,ctx);}catch(const std::exception&e){
        auto failure=object();put(failure.get(),"version",std::string(VERSION));put(failure.get(),"complete",int64_t(0));put(failure.get(),"error",std::string(e.what()));
        try{writeText(std::string(ROOT)+"/logs/save-discovery.json",encode(failure.get()));}catch(...){log("Could not save scan failure summary");}
        throw;
    }
    for(auto&e:found.errors)log("Save scan fallback: "+e);
    auto j=object();put(j.get(),"complete",int64_t(1));put(j.get(),"version",std::string(VERSION));put(j.get(),"read_records",found.readRecords);put(j.get(),"game_saves",int64_t(found.saves.size()));put(j.get(),"unresolved_records",found.unresolved);put(j.get(),"successful_readers",found.successfulReaders);
    auto errors=json_object_new_array();for(auto&e:found.errors)json_object_array_add(errors,json_object_new_string(e.c_str()));json_object_object_add(j.get(),"reader_errors",errors);
    try{writeText(std::string(ROOT)+"/logs/save-discovery.json",encode(j.get()));}catch(const std::exception&e){log(e.what());}
    if(found.saves.empty()&&found.unresolved)throw std::runtime_error("O console retornou saves, mas não foi possível identificar os jogos. A consulta foi registrada.");
    return found;
}
static std::vector<Profile> accounts()
{
    std::vector<Profile> out;AccountUid ids[8]{};s32 count{};auto rc=accountListAllUsers(ids,8,&count);
    if(R_FAILED(rc)){log("Account listing failed code="+hex(rc));return out;}
    for(int i=0;i<count;i++){
        Profile p{{ids[i].uid[0],ids[i].uid[1]},"Perfil "+std::to_string(i+1),{},false};p.key="USER_"+uid(p.uid);
        AccountProfile profile{};
        if(R_SUCCEEDED(accountGetProfile(&profile,ids[i]))){
            AccountProfileBase base{};if(R_SUCCEEDED(accountProfileGet(&profile,nullptr,&base))){base.nickname[sizeof(base.nickname)-1]=0;if(base.nickname[0])p.name=base.nickname;}accountProfileClose(&profile);
        }
        out.push_back(std::move(p));
    }
    return out;
}
std::vector<Profile> profiles(const Context&ctx)
{
    auto found=inventory(ctx);auto out=accounts();
    for(auto&g:found.saves)if(g.type==FsSaveDataType_Account){
        auto i=std::find_if(out.begin(),out.end(),[&](const Profile&p){return p.uid==g.uid;});
        if(i==out.end())out.push_back({g.uid,"Perfil adicional "+std::to_string(out.size()+1),ownerKey(g),false});
    }
    out.push_back({{},"Saves compartilhados","DEVICE",true});
    for(auto&p:out)p.saves=std::count_if(found.saves.begin(),found.saves.end(),[&](const Game&g){return owns(p,g);});
    out.insert(out.begin(),Profile{{},"Todos os saves","ALL",false,true,int(found.saves.size())});return out;
}
std::vector<Game> games(const Profile&p,const Context&ctx)
{
    auto found=inventory(ctx);auto out=selectSaves(found,p);auto users=accounts();std::map<uint64_t,std::string> names;
    for(auto&g:out){
        ctx.check();auto known=names.find(g.title);
        if(known==names.end()){
            std::string name="Jogo "+hex(g.title);auto data=std::make_unique<NsApplicationControlData>();u64 got{};NacpLanguageEntry*lang{};
            if(R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage,g.title,data.get(),sizeof(*data),&got))&&got>=sizeof(NacpStruct)&&R_SUCCEEDED(nacpGetLanguageEntry(&data->nacp,&lang))&&lang){
                lang->name[sizeof(lang->name)-1]=0;if(lang->name[0])name=lang->name;
            }
            known=names.emplace(g.title,std::move(name)).first;
        }
        g.name=known->second;g.owner=g.type==FsSaveDataType_Device?"Saves compartilhados":"Perfil adicional";
        if(g.type==FsSaveDataType_Account)for(auto&u:users)if(u.uid==g.uid){g.owner=u.name;break;}
    }
    std::sort(out.begin(),out.end(),[](const Game&a,const Game&b){return std::tie(a.name,a.owner,a.space,a.rank,a.index)<std::tie(b.name,b.owner,b.space,b.rank,b.index);});
    log("Selected saves profile="+p.key+" matched="+std::to_string(out.size())+" total="+std::to_string(found.saves.size()));return out;
}
}
