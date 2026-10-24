#pragma once
#include <string>
#include <string_view>

namespace remote
{
    inline std::string percent_encode(std::string_view input)
    {
        static constexpr char HEX[] = "0123456789ABCDEF";
        std::string out;
        for (unsigned char c : input)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            { out += static_cast<char>(c); }
            else { out += '%'; out += HEX[c >> 4]; out += HEX[c & 15]; }
        }
        return out;
    }
}
