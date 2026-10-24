#include "archive.hpp"
#include <cassert>
#include <iostream>
using namespace nx;
int main(int argc,char**argv)
{
    assert(argc==2);std::string dir=argv[1];Game g{0x010012345678ABCD,1,{0x1234,0x5678},1,0,0,"Jogo"};Context ctx{};
    auto m=checkZip(dir+"/valid.zip",&g,ctx);assert(sameSlot(m.game,g));assert(m.data==1048576);
    for(auto name:{"traversal.zip","absolute.zip","duplicate.zip","missing.zip","mismatch.zip","corrupt.zip","symlink.zip","conflict.zip","invalid-hex.zip","oversize-meta.zip","space-mismatch.zip"}){
        bool rejected=false;try{checkZip(dir+"/"+name,&g,ctx);}catch(...){rejected=true;}assert(rejected);
    }
    std::atomic_bool cancel{true};bool rejected=false;try{checkZip(dir+"/valid.zip",&g,{&cancel,{}});}catch(...){rejected=true;}assert(rejected);
    std::cout<<"Archive: CRC, paths, conflicting entries, symlinks, identity and cancellation passed.\n";
}
