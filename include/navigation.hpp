#pragma once
#include "core.hpp"
#include <cstdlib>
namespace nx {
enum class Direction { None, Up, Down, Left, Right };
struct Stick { int x{},y{}; };
inline Direction stickDirection(Stick s)
{
    constexpr int deadzone=16000;
    if(std::abs(s.x)<deadzone&&std::abs(s.y)<deadzone)return Direction::None;
    if(std::abs(s.x)>std::abs(s.y))return s.x>0?Direction::Right:Direction::Left;
    return s.y>0?Direction::Up:Direction::Down;
}
inline Direction navigationDirection(Direction dpad,Stick left,Stick right)
{
    if(dpad!=Direction::None)return dpad;
    auto d=stickDirection(left);return d!=Direction::None?d:stickDirection(right);
}
class NavigationRepeat
{
    Direction previous{Direction::None};uint64_t next{};
public:
    void reset(){previous=Direction::None;next=0;}
    Direction update(Direction d,uint64_t now)
    {
        if(d==Direction::None){reset();return d;}
        if(d!=previous){previous=d;next=now+350;return d;}
        if(now>=next){next=now+120;return d;}return Direction::None;
    }
};
inline void navigate(Model&m,Direction d)
{
    int n=count(m);if(!n||d==Direction::None)return;m.selected=std::clamp(m.selected,0,n-1);
    bool grid=m.screen==Screen::Home||m.screen==Screen::Profiles||m.screen==Screen::Games||m.screen==Screen::Cloud||m.screen==Screen::Settings||m.screen==Screen::Confirm;
    if(grid){
        if(d==Direction::Left&&m.selected%2)m.selected--;
        else if(d==Direction::Right&&m.selected%2==0&&m.selected+1<n)m.selected++;
        else if(d==Direction::Up&&m.selected>=2)m.selected-=2;
        else if(d==Direction::Down&&(m.selected/2+1)*2<n)m.selected=std::min(m.selected+2,n-1);
    }else if(m.screen==Screen::Backups){
        int delta=d==Direction::Up?-1:d==Direction::Down?1:d==Direction::Left?-6:6;
        m.selected=std::clamp(m.selected+delta,0,n-1);
    }
}
}
