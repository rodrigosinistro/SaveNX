#pragma once
#include <string>
#include <string_view>

namespace savenx
{
    inline std::size_t bounded_length(const char *s, std::size_t capacity) noexcept
    {
        std::size_t n{};
        while (n < capacity && s[n]) { ++n; }
        return n;
    }

    inline std::string plain_text(std::string_view s, std::size_t maxBytes = 511)
    {
        std::string out;
        for (std::size_t i = 0; i < s.size() && out.size() < maxBytes;)
        {
            const auto c = static_cast<unsigned char>(s[i]);
            if (c < 0x80)
            {
                ++i;
                if (c < 0x20 || c == 0x7f) { continue; }
                if (std::string_view("#*<>`^$").find(static_cast<char>(c)) != std::string_view::npos)
                { out += ' '; }
                else { out += static_cast<char>(c); }
                continue;
            }
            const unsigned n = c >= 0xf0 && c <= 0xf4 ? 4 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xc2 && c <= 0xdf ? 2 : 0;
            bool valid = n && i + n <= s.size();
            for (unsigned j = 1; valid && j < n; ++j)
            { valid = (static_cast<unsigned char>(s[i + j]) & 0xc0) == 0x80; }
            if (valid && n >= 3)
            {
                const auto d = static_cast<unsigned char>(s[i + 1]);
                valid = !(c == 0xe0 && d < 0xa0) && !(c == 0xed && d >= 0xa0) &&
                        !(c == 0xf0 && d < 0x90) && !(c == 0xf4 && d >= 0x90);
            }
            if (!valid) { ++i; continue; }
            if (out.size() + n > maxBytes) { break; }
            out.append(s.substr(i, n));
            i += n;
        }
        return out;
    }
}
