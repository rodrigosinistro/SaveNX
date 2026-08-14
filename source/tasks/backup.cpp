#include "tasks/backup.hpp"

#include "app_paths.hpp"
#include "config/config.hpp"
#include "error.hpp"
#include "fs/fs.hpp"
#include "logging/logger.hpp"
#include "remote/path.hpp"
#include "remote/remote.hpp"
#include "strings/strings.hpp"
#include "stringutil.hpp"
#include "ui/PopMessageManager.hpp"

#include <cstring>

namespace
{
    // This is used in various places for appending and checking.
    constexpr const char *STRING_ZIP_EXT = ".zip";

    // I got tired of typing out the DEFAULT_TICKS.
    constexpr int POP_TICKS = ui::PopMessageManager::DEFAULT_TICKS;
}

// Definitions at bottom.
static bool auto_backup(sys::ProgressTask *task, BackupMenuState::TaskData taskData);
static bool read_and_process_meta(const fslib::Path &targetDir, BackupMenuState::TaskData taskData, sys::ProgressTask *task);
static bool read_and_process_meta(fs::MiniUnzip &unzip, BackupMenuState::TaskData taskData, sys::ProgressTask *task);
static bool write_meta_file(const fslib::Path &target, const FsSaveDataInfo *saveInfo);
static bool write_meta_zip(fs::MiniZip &zip,
                           const data::User *user,
                           const data::TitleInfo *titleInfo,
                           const FsSaveDataInfo *saveInfo);
static fs::ScopedSaveMount create_scoped_mount(const FsSaveDataInfo *saveInfo);

void tasks::backup::create_new_backup_local(sys::threadpool::JobData taskData)
{
    // Cast data to what we actually use.
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Unpack to pointers and references for easier access.
    // Task.
    sys::ProgressTask *task = static_cast<sys::ProgressTask *>(castData->task);

    // Data.
    data::User *user               = castData->user;
    data::TitleInfo *titleInfo     = castData->titleInfo;
    const FsSaveDataInfo *saveInfo = castData->saveInfo;

    // Filesystem/path.
    const fslib::Path &path = castData->path;

    // State to update.
    BackupMenuState *spawningState = castData->spawningState;

    // Whether or not to signal completion at the end.
    const bool killTask = castData->killTask;
    castData->operationSucceeded = false;

    // If anything is invalid, bail.
    if (error::is_null(task)) { return; }
    else if (error::is_null({user, titleInfo, saveInfo}) || !path.is_valid()) { TASK_FINISH_RETURN(task); }

    // Check if the path has the zip extension. Scoped so the string doesn't linger. Not the best way to detect this btw.
    bool hasZipExt{};
    {
        const std::string pathString = path.string();
        hasZipExt                    = pathString.find(STRING_ZIP_EXT) != pathString.npos;
    }

    // If it has the zip extension
    if (hasZipExt) // At this point, this should have the zip extension appended if needed.
    {
        fs::MiniZip zip{path};
        bool backupCreated = zip.is_open() && write_meta_zip(zip, user, titleInfo, saveInfo);
        auto scopedMount = create_scoped_mount(saveInfo);
        backupCreated = backupCreated && scopedMount.is_open() && fs::copy_directory_to_zip(fs::DEFAULT_SAVE_ROOT, zip, task);
        backupCreated = zip.close() && backupCreated;
        if (!backupCreated)
        {
            error::fslib(fslib::delete_file(path));
            const char *popErrorCreating = strings::get_by_name(strings::names::BACKUPMENU_POPS, 5);
            ui::PopMessageManager::push_message(POP_TICKS, popErrorCreating);
            if (killTask) { task->complete(); }
            return;
        }
    }
    else
    {
        // Create the directory if needed.
        const bool needsDir    = !fslib::directory_exists(path);
        const bool createError = needsDir && error::fslib(fslib::create_directory(path));
        if (needsDir && createError) { TASK_FINISH_RETURN(task); }

        const bool metaWritten = write_meta_file(path, saveInfo);
        auto scopedMount = create_scoped_mount(saveInfo);
        if (!metaWritten || !scopedMount.is_open())
        {
            error::fslib(fslib::delete_directory_recursively(path));
            if (killTask) { task->complete(); }
            return;
        }
        fs::copy_directory(fs::DEFAULT_SAVE_ROOT, path, task);
    }

    // This is like this so I can reuse this code.
    castData->operationSucceeded = true;
    if (spawningState) { spawningState->refresh(); }
    if (killTask) { task->complete(); }
}

void tasks::backup::create_new_backup_remote(sys::threadpool::JobData taskData)
{
    // This is the temporary name for the backup.
    static constexpr std::string_view BACKUP_PATH = savenx::paths::TEMP_BACKUP_FILE;

    // Cast
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Unpack
    // Task.
    sys::ProgressTask *task = static_cast<sys::ProgressTask *>(castData->task);

    // Data.
    data::User *user               = castData->user;
    data::TitleInfo *titleInfo     = castData->titleInfo;
    const FsSaveDataInfo *saveInfo = castData->saveInfo;

    // Remote
    remote::Storage *remote       = remote::get_remote_storage();
    const std::string &remoteName = castData->remoteName;

    // State.
    BackupMenuState *spawningState = castData->spawningState;

    // Whether or not to signal.
    const bool killTask = castData->killTask;
    castData->operationSucceeded = false;

    // Whether or not to keep and move the backup.
    const bool keepLocal = config::get_by_key(config::keys::KEEP_LOCAL_BACKUPS);

    // Valid check.
    if (error::is_null(task)) { return; }
    else if (error::is_null({user, titleInfo, remote, saveInfo})) { TASK_FINISH_RETURN(task); }
    else if (!remote::enter_backup_directory(remote, user, titleInfo, true)) { TASK_FINISH_RETURN(task); }

    // Keep-local batch operations may not have a path prepared by a menu, so derive it from stable IDs here.
    fslib::Path zipPath{BACKUP_PATH};
    if (keepLocal)
    {
        const fslib::Path localDirectory{config::get_working_directory() / user->get_storage_key() /
                                         titleInfo->get_path_safe_title()};
        const bool directoryExists = fslib::directory_exists(localDirectory);
        const bool directoryFailed = !directoryExists && error::fslib(fslib::create_directories_recursively(localDirectory));
        if (directoryFailed)
        {
            if (killTask) { task->complete(); }
            return;
        }
        zipPath = castData->path.is_valid() ? castData->path : localDirectory / remoteName;
    }

    // Attempt to open the ZIP.
    fs::MiniZip zip{zipPath};
    if (!zip.is_open())
    {
        const char *popErrorCreating = strings::get_by_name(strings::names::BACKUPMENU_POPS, 5);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorCreating);
        TASK_FINISH_RETURN(task);
    }

    // Write metadata and every mounted save file. Never upload a partial archive.
    bool backupCreated = write_meta_zip(zip, user, titleInfo, saveInfo);
    {
        auto scopedMount = create_scoped_mount(saveInfo);
        backupCreated = backupCreated && scopedMount.is_open() &&
                        fs::copy_directory_to_zip(fs::DEFAULT_SAVE_ROOT, zip, task);
    }
    backupCreated = zip.close() && backupCreated;
    if (!backupCreated)
    {
        error::fslib(fslib::delete_file(zipPath));
        const char *popErrorCreating = strings::get_by_name(strings::names::BACKUPMENU_POPS, 5);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorCreating);
        if (killTask) { task->complete(); }
        return;
    }

    // Scoped, update status of the task.
    {
        const char *uploadFormat = strings::get_by_name(strings::names::IO_STATUSES, 5);
        std::string status       = stringutil::get_formatted_string(uploadFormat, remoteName.data());
        task->set_status(status);
    }

    // Upload the file.
    const bool uploaded = remote->upload_file(zipPath, remoteName, task);
    // Delete if desired.
    const bool deleteError = uploaded && !keepLocal && error::fslib(fslib::delete_file(zipPath));
    if (!uploaded || deleteError)
    {
        const char *popErrorUploading = strings::get_by_name(strings::names::BACKUPMENU_POPS, 10);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorUploading);
    }

    castData->operationSucceeded = uploaded || keepLocal;
    if (spawningState) { spawningState->refresh(); }
    if (killTask) { task->complete(); }
}

void tasks::backup::overwrite_backup_local(sys::threadpool::JobData taskData)
{
    // Cast
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Unpack
    // Task.
    sys::ProgressTask *task = static_cast<sys::ProgressTask *>(castData->task);

    // FS
    const fslib::Path &path = castData->path;

    // Bail if invalid.
    if (error::is_null(task) || !path.is_valid()) { return; }

    // If the backup is a directory, try to delete it. If not, delete the zip.
    const bool isDirectory = fslib::directory_exists(path);
    const bool dirFailed   = isDirectory && error::fslib(fslib::delete_directory_recursively(path));
    const bool fileFailed  = !isDirectory && error::fslib(fslib::delete_file(path));
    // If deletion of the target backup failed, pop, finish.
    if (dirFailed || fileFailed)
    {
        const char *popError = strings::get_by_name(strings::names::BACKUPMENU_POPS, 4);
        ui::PopMessageManager::push_message(POP_TICKS, popError);
        TASK_FINISH_RETURN(task);
    }

    // Ensure the new backup kills the task.
    castData->killTask = true;

    tasks::backup::create_new_backup_local(castData);
}

void tasks::backup::overwrite_backup_remote(sys::threadpool::JobData taskData)
{
    // This is the temporary path for patch backups.
    static constexpr std::string_view PATCH_PATH = savenx::paths::TEMP_PATCH_FILE;

    // Cast.
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Unpack.
    // Task.
    sys::ProgressTask *task = static_cast<sys::ProgressTask *>(castData->task);

    // Data.
    data::User *user               = castData->user;
    data::TitleInfo *titleInfo     = castData->titleInfo;
    const FsSaveDataInfo *saveInfo = castData->saveInfo;

    // Remote.
    remote::Storage *remote = remote::get_remote_storage();
    remote::Item *target    = castData->remoteItem;

    // Bail if invalid.
    if (error::is_null(task)) { return; }
    else if (error::is_null({user, titleInfo, saveInfo, remote, target})) { TASK_FINISH_RETURN(task); }

    // This is our temporary path to work with.
    const fslib::Path tempPath{PATCH_PATH};

    // Create the ZIP.
    fs::MiniZip zip{tempPath};
    if (!zip.is_open()) { TASK_FINISH_RETURN(task); }

    // Backup and close. Refuse to patch the cloud copy with a partial ZIP.
    bool backupCreated = write_meta_zip(zip, user, titleInfo, saveInfo);
    {
        auto scopedMount = create_scoped_mount(saveInfo);
        backupCreated = backupCreated && scopedMount.is_open() &&
                        fs::copy_directory_to_zip(fs::DEFAULT_SAVE_ROOT, zip, task);
    }
    backupCreated = zip.close() && backupCreated;
    if (!backupCreated)
    {
        error::fslib(fslib::delete_file(tempPath));
        const char *popErrorCreating = strings::get_by_name(strings::names::BACKUPMENU_POPS, 5);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorCreating);
        TASK_FINISH_RETURN(task);
    }

    // Scoped. Update status to uploading.
    {
        const char *targetName   = target->get_name().data();
        const char *statusFormat = strings::get_by_name(strings::names::IO_STATUSES, 5);
        std::string status       = stringutil::get_formatted_string(statusFormat, targetName);
        task->set_status(status);
    }
    // Patch the backup.
    const bool patched = remote->patch_file(target, tempPath, task);
    if (!patched)
    {
        const char *popErrorUploading = strings::get_by_name(strings::names::BACKUPMENU_POPS, 10);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorUploading);
    }

    // Delete the temporary local backup.
    const bool deleteError = error::fslib(fslib::delete_file(tempPath));
    if (deleteError)
    {
        const char *popErrorDeleting = strings::get_by_name(strings::names::BACKUPMENU_POPS, 4);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorDeleting);
    }

    task->complete();
}

void tasks::backup::restore_backup_local(sys::threadpool::JobData taskData)
{
    // Cast
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Unpack
    // Task
    sys::ProgressTask *task = static_cast<sys::ProgressTask *>(castData->task);

    // Data
    data::User *user               = castData->user;
    data::TitleInfo *titleInfo     = castData->titleInfo;
    const FsSaveDataInfo *saveInfo = castData->saveInfo;

    // FS
    const fslib::Path &path = castData->path;

    // State
    BackupMenuState *spawningState = castData->spawningState;

    // Bail on invalid data.
    if (error::is_null(task)) { return; }
    else if (error::is_null({user, titleInfo, saveInfo, spawningState})) { TASK_FINISH_RETURN(task); }

    // Get the journal size to work with.
    FsSaveDataExtraData extraData{};
    const uint8_t saveType    = saveInfo->save_data_type;
    const bool readExtra      = fs::read_save_extra_data(saveInfo, extraData);
    const int64_t journalSize = readExtra ? extraData.journal_size : titleInfo->get_journal_size(saveType);

    // Whether or not to create an auto-backup.
    const bool autoBackup = config::get_by_key(config::keys::AUTO_BACKUP_ON_RESTORE);

    // Wether or not the backup is a directory.
    const bool isDir = fslib::directory_exists(path);

    // Wether or not the it's a file and has the zip extension.
    bool hasZipExt{};
    {
        const std::string pathString = path.string();
        hasZipExt                    = !isDir && pathString.find(STRING_ZIP_EXT) != pathString.npos;
    }

    // Validate identity and container metadata before the current save is touched.
    bool metadataValid = true;
    if (!isDir && hasZipExt)
    {
        fs::MiniUnzip validationZip{path};
        if (!validationZip.is_open())
        {
            const char *popErrorOpenZip = strings::get_by_name(strings::names::EXTRASMENU_POPS, 7);
            ui::PopMessageManager::push_message(POP_TICKS, popErrorOpenZip);
            TASK_FINISH_RETURN(task);
        }
        metadataValid = read_and_process_meta(validationZip, castData, task);
    }
    else if (isDir) { metadataValid = read_and_process_meta(path, castData, task); }

    if (!metadataValid) { TASK_FINISH_RETURN(task); }

    // Create the auto-backup.
    if (autoBackup && !auto_backup(task, castData))
    {
        const char *popErrorCreating = strings::get_by_name(strings::names::BACKUPMENU_POPS, 5);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorCreating);
        TASK_FINISH_RETURN(task);
    }

    // Wipe the current save.
    {
        auto scopedMount       = create_scoped_mount(saveInfo);
        const bool deleteError = !scopedMount.is_open() || error::fslib(fslib::delete_directory_recursively(fs::DEFAULT_SAVE_ROOT));
        const bool commitError = !deleteError && error::fslib(fslib::commit_data_to_file_system(fs::DEFAULT_SAVE_MOUNT));
        if (deleteError || commitError)
        {
            const char *popErrorResetting = strings::get_by_name(strings::names::BACKUPMENU_POPS, 2);
            ui::PopMessageManager::push_message(POP_TICKS, popErrorResetting);
            TASK_FINISH_RETURN(task);
        }
    }

    // If it's not a folder and has the zip extension, try to restore as a ZIP.
    if (!isDir && hasZipExt)
    {
        fs::MiniUnzip unzip{path};
        if (!unzip.is_open())
        {
            const char *popErrorOpenZip = strings::get_by_name(strings::names::EXTRASMENU_POPS, 7);
            ui::PopMessageManager::push_message(POP_TICKS, popErrorOpenZip);
            TASK_FINISH_RETURN(task);
        }

        auto scopedMount = create_scoped_mount(saveInfo);
        const bool restored = scopedMount.is_open() &&
                              fs::copy_zip_to_directory(unzip, fs::DEFAULT_SAVE_ROOT, journalSize, task);
        if (!restored)
        {
            const char *popWriteError = strings::get_by_name(strings::names::IO_POPS, 0);
            ui::PopMessageManager::push_message(POP_TICKS, popWriteError);
            TASK_FINISH_RETURN(task);
        }
    }
    else if (isDir)
    {
        // Directory restore.
        auto scopedMount = create_scoped_mount(saveInfo);
        fs::copy_directory_commit(path, fs::DEFAULT_SAVE_ROOT, journalSize, task);
    }
    else
    {
        // Just copy the file.
        auto scopedMount = create_scoped_mount(saveInfo);
        fs::copy_file_commit(path, fs::DEFAULT_SAVE_ROOT, journalSize, task);
    }

    // Signal data was written if it previously wasn't and refresh it.
    if (spawningState)
    {
        spawningState->save_data_written();
        spawningState->refresh();
    }

    task->complete();
}

void tasks::backup::restore_backup_remote(sys::threadpool::JobData taskData)
{
    // Download path.
    static constexpr std::string_view DOWNLOAD_PATH = savenx::paths::TEMP_DOWNLOAD_FILE;

    // Cast.
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Unpack.
    // Task
    sys::ProgressTask *task = static_cast<sys::ProgressTask *>(castData->task);

    // Data.
    data::User *user               = castData->user;
    data::TitleInfo *titleInfo     = castData->titleInfo;
    const FsSaveDataInfo *saveInfo = castData->saveInfo;

    // State.
    BackupMenuState *spawningState = castData->spawningState;

    // Remote.
    remote::Storage *remote    = remote::get_remote_storage();
    const remote::Item *target = castData->remoteItem;

    // Whether or not an auto backup is needed.
    const bool autoBackup = config::get_by_key(config::keys::AUTO_BACKUP_ON_RESTORE);

    // Invalid, bail.
    if (error::is_null(task)) { return; }
    else if (error::is_null({user, titleInfo, saveInfo, spawningState, remote, target})) { TASK_FINISH_RETURN(task); }

    // Temporary file path for download.
    const fslib::Path tempPath{DOWNLOAD_PATH};

    // Scoped status update.
    {
        const char *targetName   = target->get_name().data();
        const char *statusFormat = strings::get_by_name(strings::names::IO_STATUSES, 4);
        std::string status       = stringutil::get_formatted_string(statusFormat, targetName);

        task->set_status(status);
    }

    // Download the file first. Only continue if it succeeds.
    const bool downloaded = remote->download_file(target, tempPath, task);
    if (!downloaded)
    {
        const char *popError = strings::get_by_name(strings::names::BACKUPMENU_POPS, 9);
        ui::PopMessageManager::push_message(POP_TICKS, popError);
        TASK_FINISH_RETURN(task);
    }

    // Attempt to open the downloaded file. It it fails, don't continue.
    fs::MiniUnzip backup{tempPath};
    if (!backup.is_open())
    {
        const char *popError = strings::get_by_name(strings::names::BACKUPMENU_POPS, 3);
        ui::PopMessageManager::push_message(POP_TICKS, popError);
        error::fslib(fslib::delete_file(tempPath));
        TASK_FINISH_RETURN(task);
    }

    // A remote ZIP must match the selected title, account and save slot before the current save is touched.
    if (!read_and_process_meta(backup, castData, task))
    {
        backup.close();
        error::fslib(fslib::delete_file(tempPath));
        TASK_FINISH_RETURN(task);
    }

    // Create the autobackup.
    if (autoBackup && !auto_backup(task, castData))
    {
        backup.close();
        error::fslib(fslib::delete_file(tempPath));
        const char *popErrorCreating = strings::get_by_name(strings::names::BACKUPMENU_POPS, 5);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorCreating);
        TASK_FINISH_RETURN(task);
    }

    // Clear the save container.
    {
        auto scopedMount       = create_scoped_mount(saveInfo);
        const bool deleteError = !scopedMount.is_open() ||
                                 error::fslib(fslib::delete_directory_recursively(fs::DEFAULT_SAVE_ROOT));
        const bool commitError = !deleteError &&
                                 error::fslib(fslib::commit_data_to_file_system(fs::DEFAULT_SAVE_MOUNT));
        if (deleteError || commitError)
        {
            const char *popErrorResetting = strings::get_by_name(strings::names::BACKUPMENU_POPS, 2);
            ui::PopMessageManager::push_message(POP_TICKS, popErrorResetting);
            backup.close();
            error::fslib(fslib::delete_file(tempPath));
            TASK_FINISH_RETURN(task);
        }
    }

    {
        // Get journal size.
        FsSaveDataExtraData extraData{};
        const bool readExtra      = fs::read_save_extra_data(saveInfo, extraData);
        const uint8_t saveType    = user->get_account_save_type();
        const int64_t journalSize = readExtra ? extraData.journal_size : titleInfo->get_journal_size(saveType);

        // Temp mount the save, back it up.
        auto scopedMount = create_scoped_mount(saveInfo);
        const bool restored = scopedMount.is_open() &&
                              fs::copy_zip_to_directory(backup, fs::DEFAULT_SAVE_ROOT, journalSize, task);
        if (!restored)
        {
            backup.close();
            error::fslib(fslib::delete_file(tempPath));
            const char *popWriteError = strings::get_by_name(strings::names::IO_POPS, 0);
            ui::PopMessageManager::push_message(POP_TICKS, popWriteError);
            TASK_FINISH_RETURN(task);
        }
    }
    backup.close();

    // Delete the temporary downloaded file.
    const bool deleteError = error::fslib(fslib::delete_file(tempPath));
    if (deleteError)
    {
        const char *popErrorDeleting = strings::get_by_name(strings::names::BACKUPMENU_POPS, 4);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorDeleting);
    }

    spawningState->save_data_written();
    task->complete();
}

void tasks::backup::delete_backup_local(sys::threadpool::JobData taskData)
{
    // Cast.
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Unpack
    // Task
    sys::Task *task = castData->task;

    // FS
    const fslib::Path &path = castData->path;

    // State.
    BackupMenuState *spawningState = castData->spawningState;

    // Config
    const bool trashEnabled = config::get_by_key(config::keys::ENABLE_TRASH_BIN);

    // Invalid, bail.
    if (error::is_null(task)) { return; }
    else if (error::is_null(spawningState)) { TASK_FINISH_RETURN(task); }

    // Status. This is basically a flash most of the time.
    {
        const std::string pathString = path.string();
        const char *statusFormat     = strings::get_by_name(strings::names::IO_STATUSES, 3);
        std::string status           = stringutil::get_formatted_string(statusFormat, pathString.c_str());
        task->set_status(status);
    }

    // Wether or not the backup is a folder.
    const bool isDir = fslib::directory_exists(path);

    // These errors are set in the conditions and checked later.
    bool dirError{}, fileError{};
    if (trashEnabled)
    {
        const fslib::Path newPath{config::get_working_directory() / "_TRASH_" / path.get_filename()};
        dirError  = isDir && error::fslib(fslib::rename_directory(path, newPath));
        fileError = !isDir && error::fslib(fslib::rename_file(path, newPath));
    }
    else
    {
        dirError  = isDir && error::fslib(fslib::delete_directory_recursively(path));
        fileError = !isDir && error::fslib(fslib::delete_file(path));
    }

    if (dirError || fileError)
    {
        const char *popFailed = strings::get_by_name(strings::names::BACKUPMENU_POPS, 4);
        ui::PopMessageManager::push_message(POP_TICKS, popFailed);
    }

    spawningState->refresh();
    task->complete();
}

void tasks::backup::delete_backup_remote(sys::threadpool::JobData taskData)
{
    // Cast
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Unpack
    // Task
    sys::Task *task = castData->task;

    // Remote.
    remote::Storage *remote = remote::get_remote_storage();
    remote::Item *target    = castData->remoteItem;

    // State
    BackupMenuState *spawningState = castData->spawningState;

    // Invalid, bail
    if (error::is_null(task)) { return; }
    else if (error::is_null({target, spawningState, remote})) { TASK_FINISH_RETURN(task); }

    {
        const char *targetName     = target->get_name().data();
        const char *statusTemplate = strings::get_by_name(strings::names::IO_STATUSES, 3);
        const std::string status   = stringutil::get_formatted_string(statusTemplate, targetName);
        task->set_status(status);
    }

    const bool deleted = remote->delete_item(target);
    if (!deleted)
    {
        const char *popFailed = strings::get_by_name(strings::names::BACKUPMENU_POPS, 4);
        ui::PopMessageManager::push_message(POP_TICKS, popFailed);
    }

    spawningState->refresh();
    task->complete();
}

void tasks::backup::upload_backup(sys::threadpool::JobData taskData)
{
    // Cast
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Upack.
    // Task
    sys::ProgressTask *task = static_cast<sys::ProgressTask *>(castData->task);

    // FS
    const fslib::Path &path = castData->path;

    // State
    BackupMenuState *spawningState = castData->spawningState;

    // Remote
    remote::Storage *remote = remote::get_remote_storage();

    // Invalid, bail.
    if (error::is_null(task)) { return; }
    else if (error::is_null({castData->user, castData->titleInfo, spawningState, remote})) { TASK_FINISH_RETURN(task); }
    else if (!remote::enter_backup_directory(remote, castData->user, castData->titleInfo, true))
    {
        TASK_FINISH_RETURN(task);
    }

    // Scoped status update.
    {
        const char *filename     = path.get_filename();
        const char *statusFormat = strings::get_by_name(strings::names::IO_STATUSES, 5);
        std::string status       = stringutil::get_formatted_string(statusFormat, filename);
        task->set_status(status);
    }

    const bool uploaded = remote->upload_file(path, path.get_filename(), task);
    if (!uploaded)
    {
        // We're going to pop the error, but not return since we're at the end of the function anyway.
        const char *popError = strings::get_by_name(strings::names::BACKUPMENU_POPS, 10);
        ui::PopMessageManager::push_message(POP_TICKS, popError);
    }

    spawningState->refresh();
    task->complete();
}

void tasks::backup::patch_backup(sys::threadpool::JobData taskData)
{
    // Cast
    auto castData = std::static_pointer_cast<BackupMenuState::DataStruct>(taskData);

    // Unpack
    // Task
    sys::ProgressTask *task = static_cast<sys::ProgressTask *>(castData->task);

    // FS
    const fslib::Path &path = castData->path;

    // Remote
    remote::Item *remoteItem = castData->remoteItem;
    remote::Storage *remote  = remote::get_remote_storage();

    // State.
    BackupMenuState *spawningState = castData->spawningState;

    // Invalid, bail.
    if (error::is_null(task)) { return; }
    else if (error::is_null({remoteItem, spawningState, remote})) { TASK_FINISH_RETURN(task); }

    // Status.
    {
        const char *filename     = path.get_filename();
        const char *statusFormat = strings::get_by_name(strings::names::IO_STATUSES, 5);
        std::string status       = stringutil::get_formatted_string(statusFormat, filename);
        task->set_status(status);
    }

    const bool patched = remote->patch_file(remoteItem, path, task);
    if (!patched)
    {
        const char *popErrorUploading = strings::get_by_name(strings::names::BACKUPMENU_POPS, 10);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorUploading);
    }
    task->complete();
}

static bool auto_backup(sys::ProgressTask *task, BackupMenuState::TaskData taskData)
{
    if (!taskData || !taskData->basePath) { return false; }

    // Unpack
    // Data
    data::User *user               = taskData->user;
    data::TitleInfo *titleInfo     = taskData->titleInfo;
    const FsSaveDataInfo *saveInfo = taskData->saveInfo;

    // FS
    fslib::Path &path           = taskData->path;
    const fslib::Path &basePath = *taskData->basePath;

    // Remote
    remote::Storage *remote = remote::get_remote_storage();

    // Config
    const bool autoUpload = config::get_by_key(config::keys::AUTO_UPLOAD);
    const bool exportZip  = config::get_by_key(config::keys::EXPORT_TO_ZIP);
    const bool zip        = autoUpload || exportZip;

    // Invalid, bail.
    if (error::is_null(task)) { return false; }
    else if (error::is_null({user, titleInfo, saveInfo}) || !path.is_valid()) { return false; }

    // Check if the save actually has data to backup before continuing.
    {
        auto scopedMount   = create_scoped_mount(saveInfo);
        const bool hasData = fs::directory_has_contents(fs::DEFAULT_SAVE_ROOT);
        if (!scopedMount.is_open()) { return false; }
        if (!hasData) { return true; }
    }

    // Generate the backup name.
    const char *safeNickname     = user->get_path_safe_nickname();
    const std::string dateString = stringutil::get_date_string();
    std::string backupName       = stringutil::get_formatted_string("AUTO - %s - %s", safeNickname, dateString.c_str());
    if (zip) { backupName += STRING_ZIP_EXT; }

    // Tell the backup function it shouldn't kill the task.
    taskData->killTask = false;
    taskData->operationSucceeded = false;

    // Store and swap the path so we don't lose the original.
    fslib::Path originalPath = std::move(path);
    path                     = basePath / backupName;

    if (autoUpload && remote)
    {
        // Store this for uploading.
        taskData->remoteName = std::move(backupName);

        // Create the auto backup.
        tasks::backup::create_new_backup_remote(taskData);
    }
    else { tasks::backup::create_new_backup_local(taskData); }

    // Restore the original backup path.
    taskData->path = std::move(originalPath);
    return taskData->operationSucceeded;
}

static bool read_and_process_meta(const fslib::Path &targetDir, BackupMenuState::TaskData taskData, sys::ProgressTask *task)
{
    // Unpack.
    // Data
    data::User *user               = taskData->user;
    data::TitleInfo *titleInfo     = taskData->titleInfo;
    const FsSaveDataInfo *saveInfo = taskData->saveInfo;

    // Invalid, bail.
    if (error::is_null(task)) { return false; }
    else if (error::is_null({user, titleInfo, saveInfo})) { return false; }

    // Update status.
    {
        const char *statusProcessing = strings::get_by_name(strings::names::BACKUPMENU_STATUS, 0);
        task->set_status(statusProcessing);
    }

    // Target meta path.
    const fslib::Path metaPath{targetDir / fs::NAME_SAVE_META};

    // Attempt to open file.
    fslib::File metaFile{metaPath, FsOpenMode_Read};
    if (!metaFile.is_open())
    {
        const char *popErrorProcessing = strings::get_by_name(strings::names::BACKUPMENU_POPS, 16);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorProcessing);

        return false;
    }

    // Read meta and try to process it.
    fs::SaveMetaData metaData{};
    const bool metaRead  = metaFile.read(&metaData, fs::SIZE_SAVE_META) == fs::SIZE_SAVE_META;
    const bool processed = metaRead && fs::process_save_meta_data(saveInfo, metaData);
    if (!metaRead || !processed)
    {
        const char *popErrorProcessing = strings::get_by_name(strings::names::BACKUPMENU_POPS, 11);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorProcessing);

        return false;
    }

    // If we made it here, it probably might've worked.
    return true;
}

static bool read_and_process_meta(fs::MiniUnzip &unzip, BackupMenuState::TaskData taskData, sys::ProgressTask *task)
{
    // Unpack.
    // Data.
    data::User *user               = taskData->user;
    data::TitleInfo *titleInfo     = taskData->titleInfo;
    const FsSaveDataInfo *saveInfo = taskData->saveInfo;

    // Invalid, bail.
    if (error::is_null(task)) { return false; }
    else if (error::is_null({user, titleInfo, saveInfo})) { return false; }

    // Status
    {
        const char *statusProcessing = strings::get_by_name(strings::names::BACKUPMENU_STATUS, 0);
        task->set_status(statusProcessing);
    }

    // Read every entry before touching the live save. This verifies CRCs and rejects unsafe paths.
    if (!fs::validate_zip_archive(unzip))
    {
        const char *popErrorProcessing = strings::get_by_name(strings::names::BACKUPMENU_POPS, 11);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorProcessing);
        return false;
    }

    // Try to open and read the meta file in the zip.
    fs::SaveMetaData saveMeta{};
    const bool metaFound = unzip.locate_file(fs::NAME_SAVE_META);
    if (!metaFound)
    {
        const char *popNotFound = strings::get_by_name(strings::names::BACKUPMENU_POPS, 16);
        ui::PopMessageManager::push_message(POP_TICKS, popNotFound);

        return false;
    }

    // Read & process.
    const bool metaRead      = metaFound && unzip.read(&saveMeta, fs::SIZE_SAVE_META) == fs::SIZE_SAVE_META;
    const bool metaProcessed = metaRead && fs::process_save_meta_data(saveInfo, saveMeta);
    if (!metaRead || !metaProcessed)
    {
        const char *popErrorProcessing = strings::get_by_name(strings::names::BACKUPMENU_POPS, 11);
        ui::PopMessageManager::push_message(POP_TICKS, popErrorProcessing);

        return false;
    }

    return true;
}

static bool write_meta_file(const fslib::Path &target, const FsSaveDataInfo *saveInfo)
{
    // This error is used in two places here.
    const char *popError = strings::get_by_name(strings::names::BACKUPMENU_POPS, 8);

    // Get meta for the target save info.
    fs::SaveMetaData saveMeta{};
    const fslib::Path metaPath{target / fs::NAME_SAVE_META};
    const bool hasMeta = fs::fill_save_meta_data(saveInfo, saveMeta);

    // Open for writing.
    fslib::File metaFile{metaPath, FsOpenMode_Create | FsOpenMode_Write, fs::SIZE_SAVE_META};
    if (!metaFile.is_open() || !hasMeta)
    {
        ui::PopMessageManager::push_message(POP_TICKS, popError);
        return false;
    }

    // Write the meta.
    const bool metaWritten = metaFile.write(&saveMeta, fs::SIZE_SAVE_META) == fs::SIZE_SAVE_META && metaFile.flush();
    if (!metaWritten) { ui::PopMessageManager::push_message(POP_TICKS, popError); }
    return metaWritten;
}

static bool write_meta_zip(fs::MiniZip &zip,
                           const data::User *user,
                           const data::TitleInfo *titleInfo,
                           const FsSaveDataInfo *saveInfo)
{
    const bool manifestWritten = fs::write_backup_manifest(zip, user, titleInfo, saveInfo);
    if (!manifestWritten)
    {
        logger::log("Unable to write SaveNX manifest to backup.");
    }

    // Get the meta
    fs::SaveMetaData saveMeta{};
    const bool hasMeta = fs::fill_save_meta_data(saveInfo, saveMeta);

    // Open it in the zip.
    const bool openMeta = hasMeta && zip.open_new_file(fs::NAME_SAVE_META);

    // Write it.
    const bool writeMeta = openMeta && zip.write(&saveMeta, fs::SIZE_SAVE_META);

    // Close it.
    const bool closeMeta = openMeta && zip.close_current_file();

    const bool metaWritten = hasMeta && openMeta && writeMeta && closeMeta;
    if (!metaWritten)
    {
        const char *popError = strings::get_by_name(strings::names::BACKUPMENU_POPS, 8);
        ui::PopMessageManager::push_message(POP_TICKS, popError);
    }
    return manifestWritten && metaWritten;
}

static fs::ScopedSaveMount create_scoped_mount(const FsSaveDataInfo *saveInfo)
{
    // Attempt to open the mount
    fs::ScopedSaveMount saveMount{fs::DEFAULT_SAVE_MOUNT, saveInfo};
    if (!saveMount.is_open())
    {
        const char *popError = strings::get_by_name(strings::names::BACKUPMENU_POPS, 14);
        ui::PopMessageManager::push_message(POP_TICKS, popError);
    }

    // Return it.
    return saveMount;
}
