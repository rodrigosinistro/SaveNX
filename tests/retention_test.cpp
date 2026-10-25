#include "retention.hpp"
#include <cassert>
#include <iostream>
using namespace nx;
int main()
{
    auto name=[](int day,std::string tick="123"){return "2026-10-"+std::to_string(day)+"_12-00-00_"+tick+".zip";};
    std::vector<RetentionItem> files;for(int i=20;i<25;i++)files.push_back({std::to_string(i),name(i)});
    files.push_back({"other","manual.zip"});files.push_back({"partial",name(25)+".partial"});
    auto plan=retentionPlan(files);assert(plan.keep.size()==3&&plan.remove.size()==2);assert(plan.keep[0].key=="24"&&plan.keep[2].key=="22");
    plan=retentionPlan(files,{"20"});assert(plan.keep.size()==3&&plan.remove.size()==2);for(auto&i:plan.remove)assert(i.key!="20");
    assert(backupVersion("Segurança_"+name(24))==backupVersion(name(24)));assert(!backupVersion(name(24,"001")));
    auto sameSecond=retentionPlan({{"a",name(24,"9")},{"b",name(24,"10")},{"c",name(24,"100")},{"d",name(24,"99")}});assert(sameSecond.remove[0].key=="a");
    auto duplicate=retentionPlan({{"copy1",name(24)},{"copy2",name(24)},{"copy3",name(23)},{"copy4",name(22)},{"copy5",name(21)}});assert(duplicate.keep.size()==3&&duplicate.remove.size()==2);
    for(auto&i:duplicate.keep)assert(i.name!=name(21));
    for(auto s:{"backup.zip","../2026-10-20_12-00-00_1.zip","2026-10-20_12-00-00_x.zip","2026-10-20_12-00-00_1.zip.partial","foo/2026-10-20_12-00-00_1.zip"})assert(!backupVersion(s));
    assert(retentionUser("DEVICE")&&retentionUser("USER_00000000000000010000000000000002"));assert(!retentionUser("USER_../"));
    assert(retentionTitle("TITLE_010012345678ABCD")&&!retentionTitle("photos"));
    assert(retentionSlot("TYPE_1_RANK_0_INDEX_0")&&retentionSlot("TYPE_3_RANK_1_INDEX_0_SPACE_4"));assert(!retentionSlot("TYPE_1_RANK_0_INDEX_0_"));assert(!retentionSlot("TYPE_0_RANK_0_INDEX_0"));assert(!retentionSlot("TYPE_1_RANK_0_INDEX_0/../../"));
    assert(retentionPlan({}).remove.empty());assert(retentionPlan({{"a",name(24)}}).remove.empty());
    std::cout<<"Retention: three newest versions, numeric same-second order, safety names, active-copy protection, deduplication and managed scope filters passed.\n";
}
