#pragma once
#include "fs_path.hpp"
#include <switch.h>
namespace nx::savedata {
static_assert(FsPath::capacity()==FS_MAX_PATH);
inline Result openDirectory(FsFileSystem*f,std::string_view p,u32 flags,FsDir*out){FsPath path(p);return fsFsOpenDirectory(f,path.data(),flags,out);}
inline Result openFile(FsFileSystem*f,std::string_view p,u32 flags,FsFile*out){FsPath path(p);return fsFsOpenFile(f,path.data(),flags,out);}
inline Result entryType(FsFileSystem*f,std::string_view p,FsDirEntryType*out){FsPath path(p);return fsFsGetEntryType(f,path.data(),out);}
inline Result createFile(FsFileSystem*f,std::string_view p,s64 size,u32 flags){FsPath path(p);return fsFsCreateFile(f,path.data(),size,flags);}
inline Result createDirectory(FsFileSystem*f,std::string_view p){FsPath path(p);return fsFsCreateDirectory(f,path.data());}
inline Result deleteFile(FsFileSystem*f,std::string_view p){FsPath path(p);return fsFsDeleteFile(f,path.data());}
inline Result deleteDirectoryTree(FsFileSystem*f,std::string_view p){FsPath path(p);return fsFsDeleteDirectoryRecursively(f,path.data());}
}
