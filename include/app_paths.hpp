#pragma once

#include <string_view>

namespace savenx::paths
{
    // SaveNX keeps its executable and all writable data below the Homebrew Menu app folder.
    inline constexpr std::string_view APP_ROOT   = "sdmc:/switch/SaveNX";
    inline constexpr std::string_view BACKUP_DIR = "sdmc:/switch/SaveNX/backups";
    inline constexpr std::string_view CONFIG_DIR = "sdmc:/switch/SaveNX/config";
    inline constexpr std::string_view CACHE_DIR  = "sdmc:/switch/SaveNX/cache";
    inline constexpr std::string_view LOG_DIR    = "sdmc:/switch/SaveNX/logs";
    inline constexpr std::string_view TEMP_DIR   = "sdmc:/switch/SaveNX/temp";

    inline constexpr std::string_view CONFIG_FILE       = "sdmc:/switch/SaveNX/config/SaveNX.json";
    inline constexpr std::string_view CUSTOM_PATHS_FILE = "sdmc:/switch/SaveNX/config/Paths.json";
    inline constexpr std::string_view GOOGLE_DRIVE_FILE = "sdmc:/switch/SaveNX/config/google-drive.json";
    inline constexpr std::string_view WEBDAV_FILE       = "sdmc:/switch/SaveNX/config/webdav.json";
    inline constexpr std::string_view CACHE_FILE        = "sdmc:/switch/SaveNX/cache/cache.zip";
    inline constexpr std::string_view LOG_FILE          = "sdmc:/switch/SaveNX/logs/SaveNX.log";

    inline constexpr std::string_view TEMP_BACKUP_FILE   = "sdmc:/switch/SaveNX/temp/backup.zip";
    inline constexpr std::string_view TEMP_PATCH_FILE    = "sdmc:/switch/SaveNX/temp/patch.zip";
    inline constexpr std::string_view TEMP_DOWNLOAD_FILE = "sdmc:/switch/SaveNX/temp/download.zip";

    // Paths written by SaveNX v0.1.0. They are read only by the one-time migration.
    namespace legacy
    {
        inline constexpr std::string_view BACKUP_DIR        = "sdmc:/SaveNX";
        inline constexpr std::string_view CONFIG_DIR        = "sdmc:/config/SaveNX";
        inline constexpr std::string_view CONFIG_FILE       = "sdmc:/config/SaveNX/SaveNX.json";
        inline constexpr std::string_view CUSTOM_PATHS_FILE = "sdmc:/config/SaveNX/Paths.json";
        inline constexpr std::string_view GOOGLE_DRIVE_FILE = "sdmc:/config/SaveNX/client_secret.json";
        inline constexpr std::string_view WEBDAV_FILE       = "sdmc:/config/SaveNX/webdav.json";
        inline constexpr std::string_view CACHE_FILE        = "sdmc:/config/SaveNX/cache.zip";
        inline constexpr std::string_view LOG_FILE          = "sdmc:/config/SaveNX/SaveNX.log";
        inline constexpr std::string_view TEMP_BACKUP_FILE   = "sdmc:/savenx_backup.zip";
        inline constexpr std::string_view TEMP_PATCH_FILE    = "sdmc:/savenx_patch.zip";
        inline constexpr std::string_view TEMP_DOWNLOAD_FILE = "sdmc:/savenx_download.zip";
    } // namespace legacy
} // namespace savenx::paths
