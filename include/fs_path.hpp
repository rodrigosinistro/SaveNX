#pragma once
#include "core.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
#include <string_view>
namespace nx {
// libnx fsFs* sends FS_MAX_PATH bytes, not strlen(path)+1, over IPC.
// Own the entire readable range even for '/' or a short filename.
class FsPath
{
    std::array<char,0x301> buffer{};
public:
    explicit FsPath(std::string_view path)
    {
        while(path.size()>1&&path.back()=='/')path.remove_suffix(1);
        if(path.empty()||path.front()!='/'||(path!="/"&&!relative(path.substr(1))))throw std::runtime_error("O save contém um caminho inválido.");
        if(path.size()>=buffer.size())throw std::runtime_error("Um caminho do save excede o limite do Switch.");
        std::memcpy(buffer.data(),path.data(),path.size());
    }
    const char*data()const{return buffer.data();}
    static constexpr size_t capacity(){return 0x301;}
};
}
