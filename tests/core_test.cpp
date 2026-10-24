#include "core.hpp"
#include <cassert>
#include <iostream>
using namespace nx;
int main()
{
    for(auto s:{"../a","a/../b","/a","a//b","a\\b","sdmc:/a","a/./b","a//",""})assert(!relative(s));
    assert(!relative(std::string("a\0b",3)));
    for(auto s:{"a","a/b","a/b/",".savenx/manifest.json"})assert(relative(s));
    assert(archiveEntry("save/file"));assert(archiveEntry("save/"));assert(archiveEntry(".savenx/manifest.json"));
    for(auto s:{"other/file","save/../../save/file","save//file","save/./file"})assert(!archiveEntry(s));
    assert(safeKey("TYPE_1_RANK_0_INDEX_0"));assert(!safeKey("x/y"));assert(!safeKey("x'"));
    assert(strictHex("010012345678ABCD",16));assert(!strictHex("010012345678ABCG",16));
    assert(urlEncode("a b&c=+")=="a%20b%26c%3D%2B");assert(urlEncode("ç")=="%C3%A7");
    assert(confirmedUpload(200,"id",100,100));assert(confirmedUpload(201,"id",100,100));
    for(auto code:{202,204,308,400,401,403,500})assert(!confirmedUpload(code,"id",100,100));
    assert(!confirmedUpload(200,"",100,100));assert(!confirmedUpload(200,"id",99,100));assert(!confirmedUpload(200,"id",0,0));
    Game a{1,42,{1,2},1,0,0,"A"},b=a;assert(sameSlot(a,b));b.saveId=99;assert(sameSlot(a,b));b.index=1;assert(!sameSlot(a,b));b=a;b.uid[1]=3;assert(!sameSlot(a,b));b=a;b.type=2;assert(!sameSlot(a,b));
    Model m;m.profiles={{{1,2},"Perfil","USER_1",false}};m.games={a};m.backups={{"backup.zip","","",100,false}};
    for(auto screen:{Screen::Home,Screen::Profiles,Screen::Games,Screen::Backups,Screen::Cloud,Screen::Settings,Screen::Confirm,Screen::Working,Screen::Message}){
        show(m,screen);auto before=frame(m);move(m,1);move(m,-1);assert(m.selected==0);auto after=frame(m);
        assert(before.size()==after.size());assert(!after.front().text&&after.front().x==0&&after.front().y==0&&after.front().w==1280&&after.front().h==720);
        for(auto&d:after)assert(d.x>=0&&d.y>=0&&d.w>=0&&d.h>=0&&d.x+d.w<=1280&&d.y+d.h<=720);
        for(auto&d:after)if(d.text&&d.value=="Meus jogos")assert(screen==Screen::Home);
    }
    show(m,Screen::Home);move(m,-1);assert(m.selected==3);show(m,Screen::Confirm);assert(m.selected==0);
    m.progress=-10;show(m,Screen::Working);for(auto&d:frame(m))assert(d.w>=0);
    std::cout<<"Core: paths, save identity, upload confirmation, navigation and all scene bounds passed.\n";
}
