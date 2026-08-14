#include "migration.hpp"

#include "app_paths.hpp"
#include "fslib.hpp"

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace
{
    struct FileMigration
    {
        std::string_view source;
        std::string_view destination;
    };

    struct PendingDirectory
    {
        fslib::Path source;
        fslib::Path destination;
        std::size_t parentIndex;
        bool completed{true};
    };

    constexpr std::size_t NO_PARENT = static_cast<std::size_t>(-1);

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

        // Keep traversal state on the heap. A recursive implementation can exhaust the relatively small
        // application-thread stack when legacy backups and an already-created destination share nested paths.
        std::vector<PendingDirectory> pendingDirectories{};
        pendingDirectories.push_back({source, destination, NO_PARENT});

        for (std::size_t index = 0; index < pendingDirectories.size(); ++index)
        {
            // Copy these before appending to the vector because vector growth can relocate its elements.
            const fslib::Path currentSource{pendingDirectories[index].source};
            const fslib::Path currentDestination{pendingDirectories[index].destination};
            fslib::Directory sourceDirectory{currentSource};
            if (!sourceDirectory.is_open())
            {
                pendingDirectories[index].completed = false;
                continue;
            }

            for (const fslib::DirectoryEntry &entry : sourceDirectory)
            {
                const fslib::Path oldPath{currentSource / entry};
                const fslib::Path newPath{currentDestination / entry};

                if (entry.is_directory())
                {
                    if (fslib::directory_exists(newPath))
                    {
                        pendingDirectories.push_back({oldPath, newPath, index});
                    }
                    else
                    {
                        const bool moved = fslib::rename_directory(oldPath, newPath);
                        pendingDirectories[index].completed = moved && pendingDirectories[index].completed;
                    }
                }
                else if (!fslib::file_exists(newPath))
                {
                    const bool moved = fslib::rename_file(oldPath, newPath);
                    pendingDirectories[index].completed = moved && pendingDirectories[index].completed;
                }
                else
                {
                    // Never overwrite data already created by a newer version. Keep the legacy copy for inspection.
                    pendingDirectories[index].completed = false;
                }
            }
        }

        // Delete empty sources from the leaves upward and propagate each result to its parent.
        for (std::size_t index = pendingDirectories.size(); index-- > 0;)
        {
            bool completed = pendingDirectories[index].completed;
            if (completed)
            {
                completed = fslib::delete_directory(pendingDirectories[index].source);
                pendingDirectories[index].completed = completed;
            }

            const std::size_t parentIndex = pendingDirectories[index].parentIndex;
            if (parentIndex != NO_PARENT)
            {
                pendingDirectories[parentIndex].completed = completed && pendingDirectories[parentIndex].completed;
            }
        }
        return pendingDirectories.front().completed;
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
