#include "navigation.hpp"
#include "inventory.hpp"
#include <cassert>
#include <iostream>
using namespace nx;
int main()
{
    // Actual navigation policy is shared by the native input adapter and this test.
    for(auto axis: {Stick{32767,0},Stick{-32767,0},Stick{0,32767},Stick{0,-32767}}){
        auto left=navigationDirection(Direction::None,axis,{}),right=navigationDirection(Direction::None,{},axis);
        assert(left==right&&left!=Direction::None);
    }
    assert(navigationDirection(Direction::None,{9000,-9000},{})==Direction::None);
    assert(navigationDirection(Direction::Down,{32767,0},{0,32767})==Direction::Down);
    assert(stickDirection({30000,18000})==Direction::Right);
    assert(stickDirection({18000,-30000})==Direction::Down);
    NavigationRepeat repeat;assert(repeat.update(Direction::Right,100)==Direction::Right);
    assert(repeat.update(Direction::Right,449)==Direction::None);assert(repeat.update(Direction::Right,450)==Direction::Right);
    assert(repeat.update(Direction::Right,569)==Direction::None);assert(repeat.update(Direction::Right,570)==Direction::Right);
    assert(repeat.update(Direction::Left,571)==Direction::Left);repeat.update(Direction::None,572);assert(repeat.update(Direction::Left,573)==Direction::Left);
    Model m;show(m,Screen::Home);navigate(m,Direction::Right);assert(m.selected==1);navigate(m,Direction::Right);assert(m.selected==1);
    navigate(m,Direction::Down);assert(m.selected==3);navigate(m,Direction::Left);assert(m.selected==2);navigate(m,Direction::Up);assert(m.selected==0);
    navigate(m,Direction::Left);assert(m.selected==0);navigate(m,Direction::Up);assert(m.selected==0);
    m.games.resize(5);show(m,Screen::Games);m.selected=3;navigate(m,Direction::Down);assert(m.selected==4);navigate(m,Direction::Right);assert(m.selected==4);navigate(m,Direction::Up);assert(m.selected==2);
    show(m,Screen::Confirm);navigate(m,Direction::Right);assert(m.selected==1);navigate(m,Direction::Left);assert(m.selected==0);
    show(m,Screen::Cloud);navigate(m,Direction::Right);assert(m.selected==1);
    show(m,Screen::Settings);navigate(m,Direction::Right);assert(m.selected==1);
    m.backups.resize(14);show(m,Screen::Backups);navigate(m,Direction::Right);assert(m.selected==6);navigate(m,Direction::Down);assert(m.selected==7);navigate(m,Direction::Left);assert(m.selected==1);
    Game a{0x0100000000000001,10,{1,2},1,0,0,"A"},b=a,shared=a,sd=a,system=a;
    b.saveId=11;b.uid={3,4};shared.saveId=12;shared.type=3;shared.uid={};sd.saveId=13;sd.space=4;sd.rank=1;system.saveId=14;system.type=0;system.space=0;
    auto reader=[&](const SaveQuery&q,const SaveVisitor&visit){
        if(q.mode==QueryMode::Typed&&q.space==1&&q.type==1&&q.rank==0){visit(a);visit(b);}
        if(q.mode==QueryMode::Typed&&q.space==1&&q.type==3)visit(shared);
        if(q.mode==QueryMode::Typed&&q.space==4&&q.rank==1)visit(sd);
        if(q.mode==QueryMode::Space&&q.space==1){visit(a);visit(shared);visit(system);}
    };
    auto found=scanInventory(reader,{});assert(found.saves.size()==4);
    Profile pa{{1,2},"Rodrigo","USER_1",false},pb{{3,4},"Outro","USER_2",false},device{{},"Compartilhados","DEVICE",true},all{{},"Todos","ALL",false,true};
    assert(selectSaves(found,pa).size()==2);assert(selectSaves(found,pb).size()==1);assert(selectSaves(found,device).size()==1);assert(selectSaves(found,all).size()==4);
    assert(ownerKey(a)!=ownerKey(b));assert(ownerKey(shared)=="DEVICE");assert(slot(a)!=slot(sd));assert(!sameSlot(a,sd));
    // Filtered readers may be denied while ordinary readers work, or both may return zero.
    auto fallback=scanInventory([&](const SaveQuery&q,const SaveVisitor&visit){if(q.mode==QueryMode::Typed)throw std::runtime_error("filtered denied");if(q.mode==QueryMode::Space&&q.space==1)visit(a);},{});
    assert(fallback.saves.size()==1&&fallback.errors.size()==8);
    auto global=scanInventory([&](const SaveQuery&q,const SaveVisitor&visit){if(q.mode==QueryMode::All){visit(a);visit(system);}},{});assert(global.saves.size()==1);
    auto empty=scanInventory([](const SaveQuery&,const SaveVisitor&){},{});assert(empty.saves.empty()&&empty.successfulReaders>0);
    bool failed=false;try{scanInventory([](const SaveQuery&,const SaveVisitor&){throw std::runtime_error("denied");},{});}catch(...){failed=true;}assert(failed);
    std::atomic_bool cancel{false};int calls=0;failed=false;try{scanInventory([&](const SaveQuery&,const SaveVisitor&visit){calls++;cancel=true;visit(a);},{&cancel,{}});}catch(...){failed=true;}assert(failed&&calls==1);
    // Decorative navigation must not return in any scene.
    for(auto screen:{Screen::Home,Screen::Profiles,Screen::Games,Screen::Backups,Screen::Cloud,Screen::Settings,Screen::Confirm,Screen::Working,Screen::Message}){
        show(m,screen);for(auto&d:frame(m)){assert(!(d.x==0&&d.y==0&&d.w==230&&d.h==720));assert(d.value!="Jogos e perfis");}
    }
    std::cout<<"Navigation/inventory: both sticks, four directions, repeat, grid boundaries, all profiles, save spaces, ranks, deduplication, fallbacks and cancellation passed.\n";
}
