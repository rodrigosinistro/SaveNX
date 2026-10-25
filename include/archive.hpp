#pragma once
#include "platform.hpp"
namespace nx {
struct Manifest { Game game; int64_t data{},journal{}; };
Manifest checkZip(const std::string&,const Game*,const Context&);
}
