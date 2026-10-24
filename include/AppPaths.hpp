#pragma once
#include <filesystem>
#include <string_view>
#include <system_error>

namespace savenx
{
    inline constexpr std::string_view APP_ROOT = "sdmc:/switch/SaveNX";
    inline constexpr std::string_view QUEUE_ROOT = "sdmc:/switch/SaveNX/pending_uploads";

    inline bool prepare_layout(const std::filesystem::path &root = APP_ROOT)
    {
        std::error_code ec;
        for (const char *name : {"config", "cache", "logs", "temp", "backups", "pending_uploads"})
        {
            std::filesystem::create_directories(root / name, ec);
            if (ec) { return false; }
        }
        return true;
    }
}
