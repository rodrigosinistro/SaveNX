#pragma once
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <unistd.h>

namespace savenx
{
    inline std::filesystem::path recoverable_file(const std::filesystem::path &path)
    {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) { return path; }
        return std::filesystem::path(path.string() + ".previous");
    }

    inline bool atomic_write(const std::filesystem::path &path, std::string_view bytes)
    {
        const auto temp = path.string() + ".tmp";
        FILE *out = std::fopen(temp.c_str(), "wb");
        if (!out) { return false; }
        bool ok = std::fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size();
        ok = std::fflush(out) == 0 && ok;
        if (ok) { ok = ::fsync(::fileno(out)) == 0; }
        ok = std::fclose(out) == 0 && ok;
        if (!ok) { return false; }
        std::error_code ec;
        std::filesystem::rename(temp, path, ec);
        const auto previous = path.string() + ".previous";
        if (!ec) { std::filesystem::remove(previous, ec); return true; }
        // Switch FAT rename may reject an existing destination. Keep a recoverable
        // previous version across the two renames instead of deleting the session.
        if (!std::filesystem::exists(path, ec) || ec) { return false; }
        std::filesystem::remove(previous, ec);
        ec.clear();
        std::filesystem::rename(path, previous, ec);
        if (ec) { return false; }
        std::filesystem::rename(temp, path, ec);
        if (ec)
        {
            std::error_code restore;
            std::filesystem::rename(previous, path, restore);
            return false;
        }
        std::filesystem::remove(previous, ec);
        return true;
    }
}
