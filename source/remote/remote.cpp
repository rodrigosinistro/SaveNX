#include "remote/remote.hpp"

#include "StateManager.hpp"
#include "appstates/TaskState.hpp"
#include "error.hpp"
#include "input.hpp"
#include "logging/logger.hpp"
#include "oauth_client.hpp"
#include "remote/GoogleDrive.hpp"
#include "remote/WebDav.hpp"
#include "strings/strings.hpp"
#include "stringutil.hpp"
#include "ui/PopMessageManager.hpp"

#include <chrono>
#include <ctime>
#include <memory>
#include <thread>

namespace
{
    /// @brief This is just the string for finding and creating the SaveNX dir.
    constexpr const char *STRING_SAVENX_DIR = "SaveNX";

    /// @brief This is the single (for now) instance of a storage class.
    std::unique_ptr<remote::Storage> s_storage{};

    // clang-format off
    struct DriveStruct : sys::Task::DataStruct
    {
        remote::GoogleDrive *drive{};
    };
    // clang-format on
} // namespace

static void initialize_google_drive();
static void initialize_webdav();

// Declarations here. Definitions at bottom.
/// @brief This is the thread function that handles logging into Google.
static void drive_sign_in(sys::threadpool::JobData taskData);

/// @brief This creates (if needed) the SaveNX folder for Google Drive and sets it as the root.
/// @param drive Pointer to the drive instance.
static void drive_set_savenx_root(remote::GoogleDrive *drive);

/// @brief Shows the connected Google account, falling back to the localized success message.
static void push_google_drive_success(remote::GoogleDrive *drive);

bool remote::has_internet_connection() noexcept
{
    NifmInternetConnectionType type{};
    uint32_t strength{};
    NifmInternetConnectionStatus status{};
    const bool getError = error::libnx(nifmGetInternetConnectionStatus(&type, &strength, &status));
    if (getError || status != NifmInternetConnectionStatus_Connected) { return false; }
    return true;
}

void remote::initialize(sys::threadpool::JobData jobData)
{
    const bool driveConfigExists = fslib::file_exists(remote::PATH_GOOGLE_DRIVE_CONFIG);
    const bool webdavExists      = fslib::file_exists(remote::PATH_WEBDAV_CONFIG);
    const bool embeddedDriveClient = savenx::oauth::has_embedded_google_client();

    // SaveNX 0.2.5 clean-install rule:
    // An embedded OAuth client makes Google Drive available, but it must not start a
    // first-run authorization flow from this worker thread. The old path called
    // TaskState::create_push_fade() from the thread pool even though StateManager is
    // not thread-safe, racing the data-loading/finalization state and potentially
    // leaving the application stuck on "Finalizando". A brand-new installation now
    // reaches the dashboard first. A dedicated main-thread authorization action will
    // own the first connection flow.
    if (!driveConfigExists && embeddedDriveClient && !webdavExists)
    {
        logger::log("Embedded Google OAuth client detected; first-run authorization deferred until dashboard UI.");
        return;
    }

    // Preserve an existing WebDAV-only setup. Otherwise a previously authorized
    // Google Drive configuration can reconnect automatically.
    const bool driveExists = driveConfigExists;
    if ((driveExists || webdavExists) && !remote::has_internet_connection())
    {
        const char *popNoInternet = strings::get_by_name(strings::names::REMOTE_POPS, 0);
        ui::PopMessageManager::push_message(ui::PopMessageManager::DEFAULT_TICKS, popNoInternet);
        return;
    }

    if (driveExists) { initialize_google_drive(); }
    else if (webdavExists) { initialize_webdav(); }
}

void remote::request_google_drive_authorization()
{
    const int popTicks = ui::PopMessageManager::DEFAULT_TICKS;

    // This entry point is intentionally called only by an explicit UI action. It is
    // safe to create/push TaskState here because we are on the application's main/UI
    // thread rather than the worker thread used during startup.
    if (remote::get_remote_storage())
    {
        const char *popDriveSuccess = strings::get_by_name(strings::names::GOOGLE_DRIVE, 1);
        ui::PopMessageManager::push_message(popTicks, popDriveSuccess);
        return;
    }

    if (!remote::has_internet_connection())
    {
        const char *popNoInternet = strings::get_by_name(strings::names::REMOTE_POPS, 0);
        ui::PopMessageManager::push_message(popTicks, popNoInternet);
        return;
    }

    logger::log("Starting explicit Google Drive authorization from SaveNX UI.");
    initialize_google_drive();
}

void initialize_google_drive()
{
    s_storage                  = std::make_unique<remote::GoogleDrive>();
    remote::GoogleDrive *drive = static_cast<remote::GoogleDrive *>(s_storage.get());
    if (drive->sign_in_required())
    {
        auto driveStruct   = std::make_shared<DriveStruct>();
        driveStruct->drive = drive;

        // This path is kept only for an explicit/main-thread reauthorization flow.
        // Startup no longer reaches it from the thread pool on a clean installation.
        TaskState::create_push_fade(drive_sign_in, driveStruct);
        return;
    }

    // To do: Handle this better. Maybe retry somehow?
    if (!drive->is_initialized()) { return; }

    drive_set_savenx_root(drive);
    push_google_drive_success(drive);
}

void initialize_webdav()
{
    s_storage          = std::make_unique<remote::WebDav>();
    const int popTicks = ui::PopMessageManager::DEFAULT_TICKS;
    if (s_storage->is_initialized())
    {
        const char *popDavSuccess = strings::get_by_name(strings::names::WEBDAV, 0);
        ui::PopMessageManager::push_message(popTicks, popDavSuccess);
    }
    else
    {
        const char *popDavFailed = strings::get_by_name(strings::names::WEBDAV, 1);
        ui::PopMessageManager::push_message(popTicks, popDavFailed);
    }
}

remote::Storage *remote::get_remote_storage() noexcept
{
    if (!s_storage || !s_storage->is_initialized()) { return nullptr; }
    return s_storage.get();
}

static void drive_sign_in(sys::threadpool::JobData taskData)
{
    static constexpr const char *STRING_ERROR_SIGNING_IN = "Error signing into Google Drive: %s";

    auto castData = std::static_pointer_cast<DriveStruct>(taskData);

    sys::Task *task            = castData->task;
    remote::GoogleDrive *drive = castData->drive;

    const int popTicks = ui::PopMessageManager::DEFAULT_TICKS;
    std::string message{}, deviceCode{};
    std::time_t expiration{};
    int pollingInterval{};
    if (!drive->get_sign_in_data(message, deviceCode, expiration, pollingInterval))
    {
        logger::log(STRING_ERROR_SIGNING_IN, "Getting sign in data failed!");
        TASK_FINISH_RETURN(task);
    }

    task->set_status(message);

    while (std::time(NULL) < expiration && !drive->poll_sign_in(deviceCode))
    {
        const bool bPressed = input::button_pressed(HidNpadButton_B);
        const bool bHeld    = input::button_held(HidNpadButton_B);
        if (bPressed || bHeld) { break; }

        std::this_thread::sleep_for(std::chrono::seconds(pollingInterval));
    }

    if (drive->is_initialized())
    {
        drive_set_savenx_root(drive);
        push_google_drive_success(drive);
    }
    else
    {
        const char *popDriveFailed = strings::get_by_name(strings::names::GOOGLE_DRIVE, 2);
        ui::PopMessageManager::push_message(popTicks, popDriveFailed);
    }

    task->complete();
}

static void drive_set_savenx_root(remote::GoogleDrive *drive)
{
    const bool savenxExists  = drive->directory_exists(STRING_SAVENX_DIR);
    const bool savenxCreated = !savenxExists && drive->create_directory(STRING_SAVENX_DIR);
    if (!savenxExists && !savenxCreated) { return; }

    const remote::Item *savenxDir = drive->get_directory_by_name(STRING_SAVENX_DIR);
    if (!savenxDir) { return; }

    drive->set_root_directory(savenxDir);
    drive->change_directory(savenxDir);
}

static void push_google_drive_success(remote::GoogleDrive *drive)
{
    const int popTicks = ui::PopMessageManager::DEFAULT_TICKS;
    const std::string_view accountEmail = drive->get_account_email();
    if (accountEmail.empty())
    {
        const char *popDriveSuccess = strings::get_by_name(strings::names::GOOGLE_DRIVE, 1);
        ui::PopMessageManager::push_message(popTicks, popDriveSuccess);
        return;
    }

    std::string accountMessage = stringutil::get_formatted_string("Google Drive: %s", accountEmail.data());
    ui::PopMessageManager::push_message(popTicks, accountMessage);
}
