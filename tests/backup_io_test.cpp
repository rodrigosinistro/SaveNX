#include "platform.hpp"
#include "archive.hpp"
#include <switch.h>
#include <minizip/unzip.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <fstream>
namespace fs=std::filesystem;
static fs::path gameRoot;
static int mounts{},writers{},commits{},pathCalls{};
static bool failRead{},failDirectory{};
static constexpr Result failure=0x202;
static fs::path nativePath(const char*p)
{
    // Model the SDK's 769-byte IPC input, including the whole buffer after NUL.
    char block[FS_MAX_PATH];std::memcpy(block,p,sizeof block);size_t len=std::strlen(block);
    assert(len<sizeof block&&block[0]=='/');for(size_t i=len;i<sizeof block;i++)assert(block[i]==0);
    std::string path(block);assert(path=="/"||path.back()!='/');pathCalls++;
    return gameRoot/path.substr(1);
}
Result fsOpenSaveDataFileSystem(FsFileSystem*out,FsSaveDataSpaceId,const FsSaveDataAttribute*){assert(mounts++==0);out->readOnly=false;return 0;}
Result fsOpenReadOnlySaveDataFileSystem(FsFileSystem*out,FsSaveDataSpaceId,const FsSaveDataAttribute*){assert(mounts++==0);out->readOnly=true;return 0;}
void fsFsClose(FsFileSystem*){assert(writers==0);assert(--mounts==0);}
Result fsReadSaveDataFileSystemExtraDataBySaveDataSpaceId(void*out,size_t size,FsSaveDataSpaceId,u64){assert(size==sizeof(FsSaveDataExtraData));*static_cast<FsSaveDataExtraData*>(out)={4*1024*1024,131072};return 0;}
Result fsExtendSaveDataFileSystem(FsSaveDataSpaceId,u64,s64,s64){return 0;}
Result fsFsOpenDirectory(FsFileSystem*,const char*p,u32,FsDir*out){auto path=nativePath(p);if(failDirectory)return failure;out->items=new std::vector<fs::directory_entry>;for(auto&e:fs::directory_iterator(path))out->items->push_back(e);return 0;}
Result fsDirRead(FsDir*d,s64*count,size_t max,FsDirectoryEntry*out){*count=0;while(max--&&d->index<d->items->size()){auto&e=(*d->items)[d->index++];auto&v=out[(*count)++];v={};auto name=e.path().filename().string();assert(name.size()<sizeof v.name);std::memcpy(v.name,name.c_str(),name.size()+1);v.type=e.is_directory()?FsDirEntryType_Dir:FsDirEntryType_File;v.file_size=e.is_regular_file()?e.file_size():0;}return 0;}
void fsDirClose(FsDir*d){delete d->items;d->items=nullptr;}
Result fsFsOpenFile(FsFileSystem*f,const char*p,u32 mode,FsFile*out){auto path=nativePath(p);out->writable=(mode&FsOpenMode_Write)!=0;assert(!out->writable||!f->readOnly);out->stream=new std::fstream(path,std::ios::binary|std::ios::in|(out->writable?std::ios::out:std::ios::openmode{}));if(!*out->stream){delete out->stream;out->stream=nullptr;return failure;}if(out->writable)writers++;return 0;}
Result fsFileRead(FsFile*f,s64 offset,void*out,u64 size,u32,u64*got){if(failRead)return failure;f->stream->clear();f->stream->seekg(offset);f->stream->read(static_cast<char*>(out),size);*got=f->stream->gcount();return 0;}
Result fsFileWrite(FsFile*f,s64 offset,const void*in,u64 size,u32){assert(f->writable);f->stream->seekp(offset);f->stream->write(static_cast<const char*>(in),size);return *f->stream?0:failure;}
Result fsFileFlush(FsFile*f){f->stream->flush();return *f->stream?0:failure;}
void fsFileClose(FsFile*f){assert(f->stream);delete f->stream;f->stream=nullptr;if(f->writable)writers--;}
Result fsFsGetEntryType(FsFileSystem*,const char*p,FsDirEntryType*out){auto path=nativePath(p);if(!fs::exists(path))return failure;*out=fs::is_directory(path)?FsDirEntryType_Dir:FsDirEntryType_File;return 0;}
Result fsFsCreateFile(FsFileSystem*f,const char*p,s64 size,u32){assert(!f->readOnly);auto path=nativePath(p);{std::ofstream o(path,std::ios::binary);if(!o)return failure;}fs::resize_file(path,size);return 0;}
Result fsFsCreateDirectory(FsFileSystem*f,const char*p){assert(!f->readOnly);return fs::create_directory(nativePath(p))?0:failure;}
Result fsFsDeleteFile(FsFileSystem*f,const char*p){assert(!f->readOnly);return fs::remove(nativePath(p))?0:failure;}
Result fsFsDeleteDirectoryRecursively(FsFileSystem*f,const char*p){assert(!f->readOnly);return fs::remove_all(nativePath(p))?0:failure;}
Result fsFsCommit(FsFileSystem*f){assert(!f->readOnly&&writers==0);commits++;return 0;}
static void write(const fs::path&p,const std::string&s){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write(s.data(),s.size());}
static std::string read(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static std::string zipped(const std::string&file,const char*entry){unzFile z=unzOpen64(file.c_str());assert(z);assert(unzLocateFile(z,entry,1)==UNZ_OK);assert(unzOpenCurrentFile(z)==UNZ_OK);std::string result;char buffer[8192];int n;while((n=unzReadCurrentFile(z,buffer,sizeof buffer))>0)result.append(buffer,n);assert(n==0&&unzCloseCurrentFile(z)==UNZ_OK);unzClose(z);return result;}
int main(int argc,char**argv)
{
    assert(argc==2);fs::path root=fs::absolute(argv[1])/"backup-io";fs::create_directories(root);fs::current_path(root);gameRoot=root/"game";
    std::string binary;for(int i=0;i<210000;i++)binary+=char(i%256);
    write(gameRoot/"progress.bin",binary);write(gameRoot/"nested"/"sub"/"ação.dat",std::string("A\0B",3));write(gameRoot/"empty","");fs::create_directories(gameRoot/"empty-folder");
    nx::prepare();nx::Profile p{{1,2},"Teste","USER_TEST",false};nx::Game g{0x010012345678ABCD,1,{1,2},1,0,0,"Jogo"};nx::Context ctx{};
    auto b=nx::backup(p,g,ctx);nx::checkZip(b.path,&g,ctx);assert(zipped(b.path,"save/progress.bin")==binary);assert(zipped(b.path,"save/nested/sub/ação.dat")==std::string("A\0B",3));assert(zipped(b.path,"save/empty").empty());
    assert(nx::pendingCount()==1&&nx::backupCount()==1&&mounts==0&&writers==0);assert(nx::pending()[0].file==b.path);
    for(auto failMode:{0,1}){
        failRead=failMode==0;failDirectory=failMode==1;bool rejected=false;try{nx::backup(p,g,ctx);}catch(const std::exception&e){rejected=true;assert(std::string(e.what()).find("FS 00000202")!=std::string::npos);}assert(rejected);failRead=failDirectory=false;
        assert(nx::pendingCount()==1&&nx::backupCount()==1&&mounts==0&&writers==0);
    }
    write(gameRoot/"progress.bin","NEW CURRENT SAVE");write(gameRoot/"new-file","NEW");nx::restore(p,g,b,ctx);
    assert(read(gameRoot/"progress.bin")==binary&&read(gameRoot/"nested/sub/ação.dat")==std::string("A\0B",3));assert(!fs::exists(gameRoot/"new-file")&&fs::is_directory(gameRoot/"empty-folder"));
    assert(nx::backupCount()==2&&nx::pendingCount()==1&&mounts==0&&writers==0&&commits>0&&pathCalls>15);
    for(auto&e:fs::recursive_directory_iterator(fs::path(nx::ROOT)/"backups-v2"))assert(e.path().extension()!=".partial");
    auto copies=nx::localBackups(p,g);bool safety=false;for(auto&copy:copies)if(copy.name.starts_with("Segurança_")){safety=true;assert(zipped(copy.path,"save/progress.bin")=="NEW CURRENT SAVE");}assert(safety);
    // Retention must cap the offline queue too, while preserving another profile/slot.
    auto other=g;other.uid={3,4};auto otherCopy=nx::backup(p,other,ctx);
    std::vector<nx::Backup> recent;for(int n=0;n<4;n++){write(gameRoot/"progress.bin","VERSION_"+std::to_string(n));recent.push_back(nx::backup(p,g,ctx));}
    assert(!fs::exists(recent[0].path)&&fs::exists(otherCopy.path));assert(nx::localBackups(p,g).size()==3);
    assert(nx::pendingCount()==4); // Three for this game/profile, one for the other.
    auto selected=recent[1];write(gameRoot/"progress.bin","BEFORE_RESTORE");nx::restore(p,g,selected,ctx);
    assert(read(gameRoot/"progress.bin")=="VERSION_1");assert(nx::localBackups(p,g).size()==3);assert(fs::exists(otherCopy.path));
    bool protectedSafety=false;for(auto&copy:nx::localBackups(p,g))if(copy.name.starts_with("Segurança_")){protectedSafety=true;assert(zipped(copy.path,"save/progress.bin")=="BEFORE_RESTORE");}assert(protectedSafety);
    // Existing history is organized explicitly; non-managed ZIP names are left alone.
    auto history=g;history.index=1;for(int n=0;n<5;n++)nx::backup(p,history,ctx,true,false);
    auto historyDir=nx::gameDirectory(p,history);fs::copy_file(otherCopy.path,fs::path(historyDir)/"manual.zip",fs::copy_options::overwrite_existing);
    assert(nx::localBackups(p,history).size()==6);nx::organizeLocal(ctx);assert(nx::localBackups(p,history).size()==4);assert(fs::exists(fs::path(historyDir)/"manual.zip"));
    std::cout<<"Save I/O: full SDK path buffers, nested Unicode/binary/empty files, ZIP contents, persistent queue, failed-copy cleanup, restore and safety copy passed.\n";
}
