#include "platform.hpp"
#include "json_util.hpp"
#include "archive.hpp"
#include "save_fs.hpp"
#include "retention.hpp"
#include <switch.h>
#include <minizip/zip.h>
#include <minizip/unzip.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>

namespace nx {
namespace fs=std::filesystem;
static std::string path(std::string_view s){return std::string(ROOT)+std::string(s);}
void log(const std::string&s){static std::mutex lock;std::lock_guard g(lock);std::ofstream f(path("/logs/savenx.log"),std::ios::app);f<<timestamp()<<" "<<s<<"\n";}
static void sdk(Result r,const char*what,std::string_view detail={})
{
    if(R_FAILED(r)){
        log(std::string(what)+" code="+hex(r)+(detail.empty()?"":" path="+std::string(detail)));
        throw std::runtime_error(std::string("Não foi possível ")+what+". FS "+hex(r).substr(8));
    }
}
void prepare(){std::error_code ec;for(auto*d:{"/backups-v2","/queue-v2","/config","/temp","/logs"}){fs::create_directories(path(d),ec);if(ec)throw std::runtime_error("Não foi possível preparar o cartão SD.");}}
std::string readText(const std::string&p){std::ifstream in(p,std::ios::binary);if(!in){in.clear();in.open(p+".previous",std::ios::binary);}return {std::istreambuf_iterator<char>(in),{}};}
void writeText(const std::string&p,const std::string&s)
{
    const auto temp=p+".tmp",previous=p+".previous";FILE*f=std::fopen(temp.c_str(),"wb");if(!f)throw std::runtime_error("Não foi possível salvar no cartão SD.");
    bool ok=std::fwrite(s.data(),1,s.size(),f)==s.size();ok=(std::fflush(f)==0)&&ok;ok=(std::fclose(f)==0)&&ok;
    int fd=::open(temp.c_str(),O_RDWR);if(fd>=0){ok=(::fsync(fd)==0)&&ok;::close(fd);}else ok=false;
    if(!ok)throw std::runtime_error("Falha ao confirmar o arquivo no cartão SD.");
    std::error_code ec;fs::rename(temp,p,ec);if(!ec){fs::remove(previous,ec);return;}
    if(!fs::exists(p))throw std::runtime_error("Não foi possível concluir a gravação.");
    fs::remove(previous,ec);fs::rename(p,previous,ec);if(ec)throw std::runtime_error("Não foi possível preservar o arquivo anterior.");
    fs::rename(temp,p,ec);if(ec){fs::rename(previous,p);throw std::runtime_error("Não foi possível concluir a gravação.");}fs::remove(previous,ec);
}
std::string timestamp(){std::time_t t=std::time(nullptr);std::tm tm{};if(auto*value=std::gmtime(&t))tm=*value;char out[40];std::strftime(out,sizeof out,"%Y-%m-%d_%H-%M-%S",&tm);return out;}
static AccountUid account(const std::array<uint64_t,2>&u){AccountUid a{};a.uid[0]=u[0];a.uid[1]=u[1];return a;}
struct SaveFs
{
    FsFileSystem fs{};bool opened{};
    SaveFs(const Game&g,bool write){FsSaveDataAttribute a{};a.application_id=g.title;a.uid=account(g.uid);a.save_data_type=g.type;a.save_data_rank=g.rank;a.save_data_index=g.index;
        sdk(write?fsOpenSaveDataFileSystem(&fs,static_cast<FsSaveDataSpaceId>(g.space),&a):fsOpenReadOnlySaveDataFileSystem(&fs,static_cast<FsSaveDataSpaceId>(g.space),&a),"abrir o save");opened=true;}
    ~SaveFs(){if(opened)fsFsClose(&fs);}SaveFs(const SaveFs&)=delete;
};
static std::vector<FsDirectoryEntry> entries(FsFileSystem*f,const std::string&p)
{
    FsDir dir{};sdk(savedata::openDirectory(f,p,FsDirOpenMode_ReadDirs|FsDirOpenMode_ReadFiles,&dir),"listar os arquivos do save",p);std::vector<FsDirectoryEntry> out;
    try{FsDirectoryEntry e{};s64 n{};do{sdk(fsDirRead(&dir,&n,1,&e),"ler a pasta do save");if(n)out.push_back(e);}while(n);fsDirClose(&dir);}catch(...){fsDirClose(&dir);throw;}return out;
}
static void zipAdd(zipFile z,const std::string&name,const void*data,size_t n)
{
    zip_fileinfo info{};if(zipOpenNewFileInZip64(z,name.c_str(),&info,nullptr,0,nullptr,0,nullptr,Z_DEFLATED,6,1)!=ZIP_OK)throw std::runtime_error("Falha ao criar o arquivo de backup.");
    if(zipWriteInFileInZip(z,data,n)!=ZIP_OK){zipCloseFileInZip(z);throw std::runtime_error("Falha ao gravar o backup.");}if(zipCloseFileInZip(z)!=ZIP_OK)throw std::runtime_error("Falha ao concluir o backup.");
}
static void zipDirectory(FsFileSystem*f,zipFile z,const std::string&rel,const Context&ctx,int depth=0)
{
    if(depth>40)throw std::runtime_error("A estrutura do save é muito profunda.");
    auto buffer=std::make_unique<unsigned char[]>(65536);
    for(auto&e:entries(f,"/"+rel)){ctx.check();e.name[sizeof(e.name)-1]=0;std::string next=rel+e.name;if(!relative(next))throw std::runtime_error("O save contém um caminho inválido.");
        if(e.type==FsDirEntryType_Dir){zipAdd(z,"save/"+next+"/",nullptr,0);zipDirectory(f,z,next+"/",ctx,depth+1);continue;}
        zip_fileinfo zi{};if(zipOpenNewFileInZip64(z,("save/"+next).c_str(),&zi,nullptr,0,nullptr,0,nullptr,Z_DEFLATED,6,1)!=ZIP_OK)throw std::runtime_error("Não foi possível compactar o save.");
        FsFile file{};sdk(savedata::openFile(f,"/"+next,FsOpenMode_Read,&file),"abrir o arquivo do save","/"+next);bool opened=true;
        try{for(s64 off=0;off<e.file_size;){ctx.check();u64 got{};sdk(fsFileRead(&file,off,buffer.get(),std::min<s64>(65536,e.file_size-off),FsReadOption_None,&got),"ler o save");
                if(!got||zipWriteInFileInZip(z,buffer.get(),got)!=ZIP_OK)throw std::runtime_error("O backup ficou incompleto.");off+=got;}
            fsFileClose(&file);opened=false;if(zipCloseFileInZip(z)!=ZIP_OK)throw std::runtime_error("Falha ao finalizar a compactação.");
        }catch(...){if(opened)fsFileClose(&file);zipCloseFileInZip(z);throw;}}
}

std::string gameDirectory(const Profile&,const Game&g){return path("/backups-v2/")+ownerKey(g)+"/TITLE_"+hex(g.title)+"/"+slot(g);}
Backup backup(const Profile&p,const Game&g,const Context&ctx,bool safety,bool enqueue)
{
    ctx.check();log("Backup start title="+hex(g.title)+" save="+hex(g.saveId)+" owner="+ownerKey(g)+" slot="+slot(g));if(ctx.report)ctx.report({safety?"Copiando o save atual para segurança...":"Criando uma cópia do save...",{}, {},0.12,true});
    FsSaveDataExtraData extra{};sdk(fsReadSaveDataFileSystemExtraDataBySaveDataSpaceId(&extra,sizeof extra,static_cast<FsSaveDataSpaceId>(g.space),g.saveId),"ler os dados do save");
    auto j=object();put(j.get(),"format",std::string("savenx.original.v1"));put(j.get(),"title",hex(g.title));put(j.get(),"user",uid(g.uid));put(j.get(),"type",g.type);put(j.get(),"rank",g.rank);put(j.get(),"index",g.index);put(j.get(),"data_size",extra.data_size);put(j.get(),"journal_size",extra.journal_size);put(j.get(),"game",g.name);put(j.get(),"space",g.space);
    const std::string id=timestamp()+"_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string name=(safety?"Segurança_":"")+id+".zip",dir=gameDirectory(p,g),file=dir+"/"+name,temp=file+".partial";fs::create_directories(dir);
    zipFile z=zipOpen64(temp.c_str(),APPEND_STATUS_CREATE);if(!z)throw std::runtime_error("Não foi possível criar o backup no SD.");
    try{auto meta=encode(j.get());zipAdd(z,".savenx/manifest.json",meta.data(),meta.size());{SaveFs mount(g,false);zipDirectory(&mount.fs,z,"",ctx);}
        int rc=zipClose(z,nullptr);z=nullptr;if(rc!=ZIP_OK)throw std::runtime_error("Falha ao fechar o backup.");checkZip(temp,&g,ctx);fs::rename(temp,file);
    }catch(...){if(z)zipClose(z,nullptr);std::error_code ec;fs::remove(temp,ec);throw;}
    const uint64_t size=fs::file_size(file);
    if(enqueue){auto job=object();put(job.get(),"id",id);put(job.get(),"file",file);put(job.get(),"name",name);put(job.get(),"user",ownerKey(g));put(job.get(),"title","TITLE_"+hex(g.title));put(job.get(),"slot",slot(g));put(job.get(),"size",int64_t(size));writeText(path("/queue-v2/")+id+".json",encode(job.get()));}
    if(!safety)try{pruneLocal(p,g,ctx,{file});}catch(const std::exception&e){log(std::string("Local retention deferred: ")+e.what());}
    log("Backup verified bytes="+std::to_string(size)+" file="+file);return {name,file,{},size,false};
}
// Close every writable file before committing the save filesystem.
static void ensureDirectories(FsFileSystem*fs,const std::string&rel)
{
    for(size_t slash=rel.find('/');slash!=std::string::npos;slash=rel.find('/',slash+1)){
        auto p="/"+rel.substr(0,slash);FsDirEntryType type{};
        if(R_FAILED(savedata::entryType(fs,p,&type))){sdk(savedata::createDirectory(fs,p),"recriar as pastas do save");sdk(fsFsCommit(fs),"confirmar uma pasta");}
        else if(type!=FsDirEntryType_Dir)throw std::runtime_error("O backup contém um conflito de pastas.");
    }
}
static void apply(const Game&g,const std::string&file,int64_t journal,const Context&ctx)
{
    SaveFs save(g,true);
    for(auto&e:entries(&save.fs,"/")){
        e.name[sizeof(e.name)-1]=0;std::string p="/"+std::string(e.name);
        sdk(e.type==FsDirEntryType_Dir?savedata::deleteDirectoryTree(&save.fs,p):savedata::deleteFile(&save.fs,p),"preparar o destino da restauração");
        sdk(fsFsCommit(&save.fs),"confirmar o espaço de restauração");
    }
    unzFile z=unzOpen64(file.c_str());if(!z)throw std::runtime_error("Não foi possível reabrir o backup.");auto buffer=std::make_unique<char[]>(65536);
    try{
        int state=unzGoToFirstFile(z);
        while(state==UNZ_OK){
            unz_file_info64 info{};char name[1025]{};
            if(unzGetCurrentFileInfo64(z,&info,name,sizeof name,nullptr,0,nullptr,0)!=UNZ_OK||info.size_filename>1024)throw std::runtime_error("Falha na leitura do backup.");
            std::string entry(name,info.size_filename);
            if(!archiveEntry(entry))throw std::runtime_error("Caminho inválido no backup.");
            if(entry.starts_with("save/")&&entry!="save/"){
                auto rel=entry.substr(5);ensureDirectories(&save.fs,rel);
                if(rel.back()!='/'){
                    auto target="/"+rel;sdk(savedata::createFile(&save.fs,target,info.uncompressed_size,0),"criar um arquivo do save");
                    sdk(fsFsCommit(&save.fs),"confirmar o arquivo criado");
                    FsFile f{};bool opened=false;
                    try{
                        if(unzOpenCurrentFile(z)!=UNZ_OK)throw std::runtime_error("Não foi possível extrair o arquivo.");
                        s64 off=0,since=0;size_t chunk=journal>0?std::max<int64_t>(4096,std::min<int64_t>(65536,journal/4)):65536;
                        while(true){
                            int n=unzReadCurrentFile(z,buffer.get(),chunk);if(n<0)throw std::runtime_error("Falha ao extrair o backup.");if(!n)break;
                            if(!opened){sdk(savedata::openFile(&save.fs,target,FsOpenMode_Write,&f),"abrir o destino do save");opened=true;}
                            sdk(fsFileWrite(&f,off,buffer.get(),n,FsWriteOption_None),"escrever os dados do save");off+=n;since+=n;
                            if(journal>0&&since>=journal/2){sdk(fsFileFlush(&f),"confirmar os dados");fsFileClose(&f);opened=false;sdk(fsFsCommit(&save.fs),"confirmar a restauração");since=0;}
                        }
                        if(opened){sdk(fsFileFlush(&f),"concluir o arquivo");fsFileClose(&f);opened=false;}
                        if(unzCloseCurrentFile(z)!=UNZ_OK||uint64_t(off)!=info.uncompressed_size)throw std::runtime_error("Falha de integridade durante a restauração.");
                        sdk(fsFsCommit(&save.fs),"confirmar a restauração");
                    }catch(...){if(opened)fsFileClose(&f);throw;}
                }
            }
            state=unzGoToNextFile(z);
        }
        if(state!=UNZ_END_OF_LIST_OF_FILE)throw std::runtime_error("Falha na leitura da restauração.");sdk(fsFsCommit(&save.fs),"concluir a restauração");unzClose(z);
    }catch(...){unzClose(z);throw;}(void)ctx;
}
void restore(const Profile&p,const Game&g,const Backup&b,const Context&ctx)
{
    auto m=checkZip(b.path,&g,ctx);auto safe=backup(p,g,ctx,true,false);ctx.check();
    FsSaveDataExtraData extra{};sdk(fsReadSaveDataFileSystemExtraDataBySaveDataSpaceId(&extra,sizeof extra,static_cast<FsSaveDataSpaceId>(g.space),g.saveId),"verificar o destino");
    if(m.data>extra.data_size||m.journal>extra.journal_size)sdk(fsExtendSaveDataFileSystem(static_cast<FsSaveDataSpaceId>(g.space),g.saveId,std::max<int64_t>(m.data,extra.data_size),std::max<int64_t>(m.journal,extra.journal_size)),"ajustar o espaço do save");
    if(ctx.report)ctx.report({"Restaurando o save...",{},{},0.55,false});
    try{apply(g,b.path,std::max<int64_t>(m.journal,extra.journal_size),ctx);}catch(...){log("Restore failed; restoring safety copy "+safe.path);try{apply(g,safe.path,extra.journal_size,ctx);}catch(...){throw std::runtime_error("A restauração falhou. A cópia de segurança está no SD.");}throw std::runtime_error("A restauração falhou; o save anterior foi recuperado.");}
    try{pruneLocal(p,g,ctx,{safe.path});}catch(const std::exception&e){log(std::string("Retention after restore deferred: ")+e.what());}
}
std::vector<Backup> localBackups(const Profile&p,const Game&g)
{
    std::vector<Backup> out;std::error_code ec;fs::directory_iterator i(gameDirectory(p,g),ec),end;if(ec)return out;
    for(;i!=end;i.increment(ec)){if(ec)break;if(i->is_regular_file()&&i->path().extension()==".zip")out.push_back({i->path().filename().string(),i->path().string(),{},i->file_size(),false});}
    std::sort(out.begin(),out.end(),[](auto&a,auto&b){return backupNewer({a.path,a.name},{b.path,b.name});});return out;
}
static int pruneDirectory(const std::string&dir,const Context&ctx,const std::set<std::string>&protectedFiles)
{
    std::vector<RetentionItem> items;std::error_code ec;fs::directory_iterator i(dir,ec),end;if(ec){if(!fs::exists(dir))return 0;throw std::runtime_error("Não foi possível consultar as cópias locais.");}
    for(;i!=end;++i)if(!i->is_symlink()&&i->is_regular_file()&&i->file_size()>0&&backupVersion(i->path().filename().string()))items.push_back({i->path().string(),i->path().filename().string()});
    auto plan=retentionPlan(std::move(items),protectedFiles);auto jobs=pending();int removed=0;
    for(auto&old:plan.remove){ctx.check();if(!fs::remove(old.key))continue;removed++;log("Local retention removed "+old.key);
        for(auto&job:jobs)if(job.file==old.key)acknowledge(job);
    }return removed;
}
int pruneLocal(const Profile&p,const Game&g,const Context&ctx,const std::set<std::string>&protectedFiles)
{
    return pruneDirectory(gameDirectory(p,g),ctx,protectedFiles);
}
int organizeLocal(const Context&ctx)
{
    int removed=0;auto root=path("/backups-v2");
    for(auto&u:fs::directory_iterator(root)){
        ctx.check();if(u.is_symlink()||!u.is_directory()||!retentionUser(u.path().filename().string()))continue;
        for(auto&t:fs::directory_iterator(u.path())){
            if(t.is_symlink()||!t.is_directory()||!retentionTitle(t.path().filename().string()))continue;
            for(auto&s:fs::directory_iterator(t.path()))if(!s.is_symlink()&&s.is_directory()&&retentionSlot(s.path().filename().string()))removed+=pruneDirectory(s.path().string(),ctx,{});
        }
    }return removed;
}

std::vector<Job> pending()
{
    std::vector<Job> out;std::error_code ec;fs::directory_iterator i(path("/queue-v2"),ec),end;if(ec)return out;
    for(;i!=end;i.increment(ec)){if(ec)break;if(i->path().extension()!=".json")continue;auto j=parse(readText(i->path().string()));if(!j)continue;
        Job job{str(j.get(),"id"),str(j.get(),"file"),str(j.get(),"name"),str(j.get(),"user"),str(j.get(),"title"),str(j.get(),"slot"),uint64_t(num(j.get(),"size"))};
        if(!safeKey(job.id)||!job.file.starts_with(path("/backups-v2/"))||!relative(job.file.substr(path("/backups-v2/").size()))||!safeKey(job.user)||!safeKey(job.title)||!safeKey(job.slot))continue;
        if(fs::exists(job.file)&&fs::file_size(job.file)==job.size)out.push_back(job);}
    return out;
}
void acknowledge(const Job&j){if(!safeKey(j.id))throw std::runtime_error("Pendência inválida.");fs::remove(path("/queue-v2/")+j.id+".json");}
int pendingCount(){return pending().size();}
int backupCount(){int n=0;std::error_code ec;fs::recursive_directory_iterator i(path("/backups-v2"),ec),end;if(ec)return 0;for(;i!=end;i.increment(ec)){if(ec)break;if(i->is_regular_file()&&i->path().extension()==".zip")++n;}return n;}
}
