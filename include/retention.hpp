#pragma once
#include "core.hpp"
#include <set>
#include <optional>
#include <functional>
namespace nx {
inline constexpr size_t BACKUP_LIMIT=3;
struct RetentionItem { std::string key,name; };
struct RetentionPlan { std::vector<RetentionItem> keep,remove; };
inline std::optional<std::string> backupVersion(std::string_view name)
{
    constexpr std::string_view safety="Segurança_";
    if(name.starts_with(safety))name.remove_prefix(safety.size());
    if(!name.ends_with(".zip"))return {};name.remove_suffix(4);
    if(name.size()<21||name.size()>60)return {};
    for(size_t i=0;i<name.size();i++){
        char expected=i==4||i==7||i==13||i==16?'-':i==10||i==19?'_':0;
        if(expected?name[i]!=expected:(name[i]<'0'||name[i]>'9'))return {};
    }
    return std::string(name);
}
inline bool versionNewer(std::string_view a,std::string_view b)
{
    auto ad=a.substr(0,19),bd=b.substr(0,19);if(ad!=bd)return ad>bd;
    a.remove_prefix(20);b.remove_prefix(20);
    while(a.size()>1&&a.front()=='0')a.remove_prefix(1);while(b.size()>1&&b.front()=='0')b.remove_prefix(1);
    return a.size()!=b.size()?a.size()>b.size():a>b;
}
inline bool backupNewer(const RetentionItem&a,const RetentionItem&b)
{
    auto av=backupVersion(a.name),bv=backupVersion(b.name);
    if(av&&bv&&*av!=*bv)return versionNewer(*av,*bv);
    if(bool(av)!=bool(bv))return bool(av);return a.key>b.key;
}
inline RetentionPlan retentionPlan(std::vector<RetentionItem> items,const std::set<std::string>&protectedKeys={})
{
    std::erase_if(items,[](auto&i){return !backupVersion(i.name);});std::sort(items.begin(),items.end(),backupNewer);
    RetentionPlan out;std::set<std::string> keptKeys,versions;
    for(auto&i:items)if(protectedKeys.contains(i.key)){out.keep.push_back(i);keptKeys.insert(i.key);versions.insert(*backupVersion(i.name));}
    for(auto&i:items){if(keptKeys.contains(i.key))continue;auto version=*backupVersion(i.name);
        if(versions.size()<BACKUP_LIMIT&&!versions.contains(version)){out.keep.push_back(i);versions.insert(version);keptKeys.insert(i.key);}else out.remove.push_back(i);}
    return out;
}
inline bool retentionUser(std::string_view s){return s=="DEVICE"||(s.starts_with("USER_")&&strictHex(s.substr(5),32));}
inline bool retentionTitle(std::string_view s){return s.starts_with("TITLE_")&&strictHex(s.substr(6),16);}
inline bool retentionSlot(std::string_view s)
{
    if(s.empty()||s.back()=='_')return false;
    std::vector<std::string_view> parts;while(!s.empty()){auto n=s.find('_');parts.push_back(s.substr(0,n));if(n==s.npos)break;s.remove_prefix(n+1);}
    if(parts.size()!=6&&parts.size()!=8)return false;
    auto digits=[](std::string_view v){return !v.empty()&&v.size()<=5&&std::all_of(v.begin(),v.end(),[](char c){return c>='0'&&c<='9';});};
    return parts[0]=="TYPE"&&(parts[1]=="1"||parts[1]=="3")&&parts[2]=="RANK"&&digits(parts[3])&&parts[4]=="INDEX"&&digits(parts[5])&&(parts.size()==6||(parts[6]=="SPACE"&&parts[7]=="4"));
}
}
