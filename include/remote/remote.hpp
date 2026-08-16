#pragma once
#include "app_paths.hpp"
#include "remote/Storage.hpp"
#include "sys/threadpool.hpp"

#include <memory>

namespace remote
{
    // Both of these are needed in two different places.
    static constexpr std::string_view PATH_GOOGLE_DRIVE_CONFIG = savenx::paths::GOOGLE_DRIVE_FILE;
    static constexpr std::string_view PATH_WEBDAV_CONFIG       = savenx::paths::WEBDAV_FILE;

    /// @brief Returns whether or not the console has an active internet connection.
    bool has_internet_connection() noexcept;

    /// @brief Initializes the remote service according to the config on the sdmc.
    void initialize(sys::threadpool::JobData jobData);

    /// @brief Starts the Google Drive device authorization flow from the UI/main thread.
    void request_google_drive_authorization();

    /// @brief Returns the pointer to the Storage instance.
    remote::Storage *get_remote_storage() noexcept;
} // namespace remote
