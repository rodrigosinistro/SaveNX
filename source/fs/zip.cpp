#include "fs/zip.hpp"

#include "config/config.hpp"
#include "error.hpp"
#include "fs/BackupManifest.hpp"
#include "fs/PathSafety.hpp"
#include "fs/SaveMetaData.hpp"
#include "logging/logger.hpp"
#include "strings/strings.hpp"
#include "stringutil.hpp"
#include "sys/sys.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>

namespace
{
    /// @brief Buffer size used for writing files to ZIP.
    constexpr size_t SIZE_ZIP_BUFFER = 0x10000;

    /// @brief Buffer size used for decompressing files from ZIP.
    constexpr size_t SIZE_UNZIP_BUFFER = 0x80000;

} // namespace

bool fs::copy_directory_to_zip(const fslib::Path &source, fs::MiniZip &dest, sys::ProgressTask *task)
{
    const char *ioStatus = strings::get_by_name(strings::names::IO_STATUSES, 1);

    fslib::Directory sourceDir{source};
    if (error::fslib(sourceDir.is_open())) { return false; }

    for (const fslib::DirectoryEntry &entry : sourceDir)
    {
        const fslib::Path fullSource{source / entry};
        if (entry.is_directory())
        {
            if (!dest.add_directory(fullSource.string()) || !fs::copy_directory_to_zip(fullSource, dest, task))
            {
                return false;
            }
        }
        else
        {
            fslib::File sourceFile{fullSource, FsOpenMode_Read};
            const std::string sourceString = fullSource.string();
            const bool newZipFile          = dest.open_new_file(sourceString);
            if (error::fslib(sourceFile.is_open()) || !newZipFile) { return false; }

            const int64_t fileSize = sourceFile.get_size();
            if (task)
            {
                std::string status = stringutil::get_formatted_string(ioStatus, sourceString.c_str());
                task->set_status(status);
                task->reset(static_cast<double>(fileSize));
            }

            std::array<unsigned char, SIZE_ZIP_BUFFER> buffer{};
            for (int64_t i = 0; i < fileSize;)
            {
                const size_t requestSize = std::min<int64_t>(buffer.size(), fileSize - i);
                const ssize_t readSize = sourceFile.read(buffer.data(), requestSize);
                if (readSize <= 0 || !dest.write(buffer.data(), static_cast<size_t>(readSize)))
                {
                    dest.close_current_file();
                    return false;
                }

                i += readSize;

                if (task) { task->update_current(static_cast<double>(i)); }
            }
            if (!dest.close_current_file()) { return false; }
        }
    }
    return true;
}

bool fs::copy_zip_to_directory(fs::MiniUnzip &unzip,
                               const fslib::Path &dest,
                               int64_t journalSize,
                               sys::ProgressTask *task)
{
    if (!unzip.reset()) { return false; }
    const char *statusTemplate  = strings::get_by_name(strings::names::IO_STATUSES, 2);
    const bool needCommits      = journalSize > 0;
    std::array<unsigned char, SIZE_UNZIP_BUFFER> buffer{};

    do {
        const std::string_view filename = unzip.get_filename();
        if (filename == fs::NAME_SAVE_META || filename == fs::NAME_BACKUP_MANIFEST) { continue; }
        else if (unzip.is_directory())
        {
            const fslib::Path dirPath{dest / unzip.get_filename()};
            const bool exists = fslib::directory_exists(dirPath);
            const bool createError = !exists && error::fslib(fslib::create_directories_recursively(dirPath));
            if (createError) { return false; }
            continue;
        }

        fslib::Path fullDest{dest / unzip.get_filename()};
        const size_t lastDir = fullDest.find_last_of('/');
        if (lastDir == fullDest.NOT_FOUND) { return false; }

        if (lastDir > 0)
        {
            const fslib::Path dirPath{fullDest.sub_path(lastDir)};
            const bool isValid     = dirPath.is_valid();
            const bool exists      = isValid && fslib::directory_exists(dirPath);
            const bool createError = isValid && !exists && error::fslib(fslib::create_directories_recursively(dirPath));
            const bool commitError = isValid && !exists && !createError &&
                                     error::fslib(fslib::commit_data_to_file_system(dirPath.get_device_name()));
            if (!isValid || createError || commitError) { return false; }
        }

        const int64_t fileSize = unzip.get_uncompressed_size();
        fslib::File destFile{fullDest, FsOpenMode_Create | FsOpenMode_Write, fileSize};
        if (error::fslib(destFile.is_open())) { return false; }

        if (task)
        {
            std::string status = stringutil::get_formatted_string(statusTemplate, fullDest.get_filename());
            task->set_status(status);
            task->reset(static_cast<double>(fileSize));
        }

        int64_t journalCount{};
        for (int64_t i = 0; i < fileSize;)
        {
            if (needCommits && journalCount >= journalSize)
            {
                if (!destFile.flush()) { return false; }
                destFile.close();
                const bool commitError = error::fslib(fslib::commit_data_to_file_system(dest.get_device_name()));
                if (commitError) { return false; }

                destFile.open(fullDest, FsOpenMode_Write);
                if (!destFile.is_open()) { return false; }
                destFile.seek(i, destFile.BEGINNING);
                journalCount = 0;
            }

            size_t requestSize = std::min<int64_t>(buffer.size(), fileSize - i);
            if (needCommits)
            {
                requestSize = std::min<int64_t>(requestSize, journalSize - journalCount);
            }
            const ssize_t readSize = unzip.read(buffer.data(), requestSize);
            if (readSize <= 0) { return false; }

            const ssize_t writeSize = destFile.write(buffer.data(), static_cast<size_t>(readSize));
            if (writeSize != readSize) { return false; }

            i += readSize;
            journalCount += readSize;

            if (task) { task->update_current(static_cast<double>(i)); }
        }
        if (!destFile.flush()) { return false; }
        destFile.close();

        const bool entryClosed = unzip.close_current_file();
        const bool commitError = error::fslib(fslib::commit_data_to_file_system(dest.get_device_name()));
        if (!entryClosed || commitError) { return false; }
    } while (unzip.next_file());
    return true;
}

bool fs::zip_has_contents(const fslib::Path &zipPath)
{
    fs::MiniUnzip unzip{zipPath};
    if (!unzip.is_open()) { return false; }

    do {
        const std::string_view filename = unzip.get_filename();
        if (filename != fs::NAME_SAVE_META && filename != fs::NAME_BACKUP_MANIFEST) { return true; }
    } while (unzip.next_file());
    return false;
}

bool fs::validate_zip_archive(fs::MiniUnzip &unzip)
{
    if (!unzip.is_open() || !unzip.reset()) { return false; }

    std::array<unsigned char, SIZE_ZIP_BUFFER> buffer{};
    bool valid = true;
    bool hasSaveEntry{};
    do {
        const std::string_view filename = unzip.get_filename();
        if (!fs::zip_path_is_safe(filename))
        {
            valid = false;
            break;
        }
        if (filename != fs::NAME_SAVE_META && filename != fs::NAME_BACKUP_MANIFEST) { hasSaveEntry = true; }

        uint64_t totalRead{};
        const uint64_t expectedSize = unzip.get_uncompressed_size();
        while (totalRead < expectedSize)
        {
            const size_t requestSize = std::min<uint64_t>(buffer.size(), expectedSize - totalRead);
            const ssize_t readSize = unzip.read(buffer.data(), requestSize);
            if (readSize <= 0)
            {
                valid = false;
                break;
            }
            totalRead += static_cast<uint64_t>(readSize);
        }

        const bool entryClosed = unzip.close_current_file();
        if (!valid || totalRead != expectedSize || !entryClosed)
        {
            valid = false;
            break;
        }
    } while (unzip.next_file());

    const bool reset = unzip.reset();
    return valid && hasSaveEntry && reset;
}
