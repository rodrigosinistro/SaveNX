#pragma once
// Host-only fake of the small SDK surface used by platform.cpp.
#include <cstdint>
#include <vector>
#include <filesystem>
#include <fstream>
using u8=uint8_t;using u32=uint32_t;using u64=uint64_t;using s64=int64_t;using Result=uint32_t;
#define R_FAILED(r) ((r)!=0)
#define R_SUCCEEDED(r) ((r)==0)
#define FS_MAX_PATH 0x301
struct AccountUid {u64 uid[2]{};};
enum FsSaveDataSpaceId {FsSaveDataSpaceId_User=1,FsSaveDataSpaceId_SdUser=4};
enum FsDirEntryType {FsDirEntryType_Dir=0,FsDirEntryType_File=1};
enum {FsDirOpenMode_ReadDirs=1,FsDirOpenMode_ReadFiles=2,FsOpenMode_Read=1,FsOpenMode_Write=2,FsReadOption_None=0,FsWriteOption_None=0};
struct FsSaveDataAttribute {u64 application_id{};AccountUid uid{};int save_data_type{},save_data_rank{},save_data_index{};};
struct FsSaveDataExtraData {s64 data_size{},journal_size{};};
struct FsFileSystem {bool readOnly{};};
struct FsDirectoryEntry {char name[FS_MAX_PATH]{};int type{};s64 file_size{};};
struct FsDir {std::vector<std::filesystem::directory_entry>*items{};size_t index{};};
struct FsFile {std::fstream*stream{};bool writable{};};
Result fsOpenSaveDataFileSystem(FsFileSystem*,FsSaveDataSpaceId,const FsSaveDataAttribute*);
Result fsOpenReadOnlySaveDataFileSystem(FsFileSystem*,FsSaveDataSpaceId,const FsSaveDataAttribute*);
void fsFsClose(FsFileSystem*);
Result fsReadSaveDataFileSystemExtraDataBySaveDataSpaceId(void*,size_t,FsSaveDataSpaceId,u64);
Result fsExtendSaveDataFileSystem(FsSaveDataSpaceId,u64,s64,s64);
Result fsFsOpenDirectory(FsFileSystem*,const char*,u32,FsDir*);
Result fsDirRead(FsDir*,s64*,size_t,FsDirectoryEntry*);
void fsDirClose(FsDir*);
Result fsFsOpenFile(FsFileSystem*,const char*,u32,FsFile*);
Result fsFileRead(FsFile*,s64,void*,u64,u32,u64*);
Result fsFileWrite(FsFile*,s64,const void*,u64,u32);
Result fsFileFlush(FsFile*);
void fsFileClose(FsFile*);
Result fsFsGetEntryType(FsFileSystem*,const char*,FsDirEntryType*);
Result fsFsCreateFile(FsFileSystem*,const char*,s64,u32);
Result fsFsCreateDirectory(FsFileSystem*,const char*);
Result fsFsDeleteFile(FsFileSystem*,const char*);
Result fsFsDeleteDirectoryRecursively(FsFileSystem*,const char*);
Result fsFsCommit(FsFileSystem*);
