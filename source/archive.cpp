#include "archive.hpp"
#include "json_util.hpp"
#include <minizip/unzip.h>
#include <memory>
#include <set>
namespace nx {
Manifest checkZip(const std::string&file,const Game*expected,const Context&ctx)
{
    unzFile z=unzOpen64(file.c_str());if(!z)throw std::runtime_error("Esse arquivo não é um backup SaveNX válido.");
    Manifest m;int manifests=0;std::set<std::string> names,files,dirs;auto buffer=std::make_unique<char[]>(65536);uint64_t total{};size_t count{};
    try{int state=unzGoToFirstFile(z);while(state==UNZ_OK){ctx.check();unz_file_info64 info{};char name[1025]{};
        if(unzGetCurrentFileInfo64(z,&info,name,sizeof name,nullptr,0,nullptr,0)!=UNZ_OK||info.size_filename>1024)throw std::runtime_error("Nome inválido no backup.");
        std::string entry(name,info.size_filename);if(!archiveEntry(entry)||!names.insert(entry).second||++count>100000||info.uncompressed_size>(16ull<<30)||total>(16ull<<30)-info.uncompressed_size)throw std::runtime_error("O backup contém caminhos ou tamanhos inválidos.");total+=info.uncompressed_size;
        if(entry.starts_with("save/")&&entry!="save/"){
            auto key=entry.substr(5);bool directory=key.back()=='/';if(directory)key.pop_back();
            for(size_t slash=key.find('/');slash!=std::string::npos;slash=key.find('/',slash+1)){
                auto parent=key.substr(0,slash);if(files.contains(parent))throw std::runtime_error("Conflito de caminhos no backup.");dirs.insert(parent);
            }
            if((directory&&files.contains(key))||(!directory&&dirs.contains(key))||(directory&&info.uncompressed_size))throw std::runtime_error("Conflito de arquivos no backup.");
            if(directory)dirs.insert(key);else files.insert(key);
        }
        if(((info.external_fa>>16)&0170000)==0120000)throw std::runtime_error("O backup contém um link inválido.");
        if(unzOpenCurrentFile(z)!=UNZ_OK)throw std::runtime_error("Não foi possível abrir uma parte do backup.");std::string meta;
        while(true){int n=unzReadCurrentFile(z,buffer.get(),65536);if(n<0)throw std::runtime_error("O backup está corrompido.");if(!n)break;ctx.check();if(entry==".savenx/manifest.json"){if(meta.size()+n>65536)throw std::runtime_error("Metadados muito grandes.");meta.append(buffer.get(),n);}}
        if(unzCloseCurrentFile(z)!=UNZ_OK)throw std::runtime_error("A verificação do backup falhou.");
        if(entry==".savenx/manifest.json"){manifests++;auto j=parse(meta);if(!j||str(j.get(),"format")!="savenx.original.v1")throw std::runtime_error("Formato de backup não reconhecido.");
            auto tid=str(j.get(),"title"),u=str(j.get(),"user");if(!strictHex(tid,16)||!strictHex(u,32))throw std::runtime_error("Identificação de backup inválida.");
            m.game.title=std::stoull(tid,nullptr,16);m.game.uid={std::stoull(u.substr(16),nullptr,16),std::stoull(u.substr(0,16),nullptr,16)};
            m.game.space=field(j.get(),"space")?num(j.get(),"space"):1;m.game.type=num(j.get(),"type");m.game.rank=num(j.get(),"rank");m.game.index=num(j.get(),"index");m.data=num(j.get(),"data_size");m.journal=num(j.get(),"journal_size");}
        state=unzGoToNextFile(z);}
        if(state!=UNZ_END_OF_LIST_OF_FILE||manifests!=1||m.data<0||m.journal<0||m.data>(16ll<<30)||m.journal>(16ll<<30))throw std::runtime_error("O backup está incompleto.");
        if(expected&&!sameSlot(m.game,*expected))throw std::runtime_error("Esse backup pertence a outro jogo, perfil ou espaço de save.");unzClose(z);return m;
    }catch(...){unzClose(z);throw;}
}
}
