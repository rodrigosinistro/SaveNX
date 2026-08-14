#include "migration.hpp"

#include "app_paths.hpp"
#include "fslib.hpp"

#include <array>
#include <string_view>

namespace
{
    struct FileMigration
    {
        std::string_view source;
        std::string_view destination;
    };

    bool move_file_without_overwrite(std::string_view source, std::string_view destination)
    {
        const fslib::Path oldPath{source};
        if (!fslib::file_exists(oldPath)) { return true; }

        const fslib::Path newPath{destination};
        if (fslib::file_exists(newPath)) { return true; }

        return fslib::rename_file(oldPath, newPath);
    }

    bool merge_directory_without_overwrite(const fslib::Path &source, const fslib::Path &destination)
    {
        if (!fslib::directory_exists(source)) { return true; }

        if (!fslib::directory_exists(destination)) { return fslib::rename_directory(source, destination); }

        fslib::Directory sourceDirectory{source};
        if (!sourceDirectory.is_open()) { return false; }

        bool completed = true;
        for (const fslib::DirectoryEntry &entry : sourceDirectory)
        {
            const fslib::Path oldPath{source / entry};
            const fslib::Path newPath{destination / entry};

            if (entry.is_directory())
            {
                const bool moved = merge_directory_without_overwrite(oldPath, newPath);
                completed        = moved && completed;
            }
            else if (!fslib::file_exists(newPath))
            {
                const bool moved = fslib::rename_file(oldPath, newPath);
                completed        = moved && completed;
            }
            else
            {
                // Never overwrite data already created by v0.1.1. Keep the legacy copy for manual inspection.
                completed = false;
            }
        }

        // Removing an empty legacy directory is safe. Failure simply leaves it available for inspection.
        if (completed) { completed = fslib::delete_directory(source); }
        return completed;
    }
} // namespace

bool migration::migrate_v0_1_0_layout()
{
    using namespace savenx::paths;

    static constexpr std::array<FileMigration, 9> FILES = {{
        {legacy::CONFIG_FILE, CONFIG_FILE},
        {legacy::CUSTOM_PATHS_FILE, CUSTOM_PATHS_FILE},
        {legacy::GOOGLE_DRIVE_FILE, GOOGLE_DRIVE_FILE},
        {legacy::WEBDAV_FILE, WEBDAV_FILE},
        {legacy::CACHE_FILE, CACHE_FILE},
        {legacy::LOG_FILE, LOG_FILE},
        {legacy::TEMP_BACKUP_FILE, TEMP_BACKUP_FILE},
        {legacy::TEMP_PATCH_FILE, TEMP_PATCH_FILE},
        {legacy::TEMP_DOWNLOAD_FILE, TEMP_DOWNLOAD_FILE},
    }};

    bool completed = true;
    for (const FileMigration &file : FILES)
    {
        const bool moved = move_file_without_overwrite(file.source, file.destination);
        completed        = moved && completed;
    }

    // This succeeds only when every legacy config file was moved and the directory is now empty.
    const fslib::Path legacyConfigDir{legacy::CONFIG_DIR};
    if (fslib::directory_exists(legacyConfigDir))
    {
        const bool removed = fslib::delete_directory(legacyConfigDir);
        completed          = removed && completed;
    }

    const bool backupsMoved = merge_directory_without_overwrite(fslib::Path{legacy::BACKUP_DIR},
                                                                 fslib::Path{BACKUP_DIR});
    completed               = backupsMoved && completed;
    return completed;
}
