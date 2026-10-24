#pragma once
#include "remote/Storage.hpp"
#include "sys/threadpool.hpp"

#include <memory>

namespace remote
{
    // Both of these are needed in two different places.
    static constexpr std::string_view PATH_GOOGLE_DRIVE_CONFIG = "sdmc:/switch/SaveNX/config/google-drive.json";
    static constexpr std::string_view PATH_WEBDAV_CONFIG       = "sdmc:/switch/SaveNX/config/webdav.json";

    /// @brief Returns whether or not the console has an active internet connection.
    bool has_internet_connection() noexcept;

    /// @brief Initializes the remote service according to the config on the sdmc.
    void initialize(sys::threadpool::JobData jobData);

    // UI entry points are called only on the main thread. Network work stays in TaskState.
    void connect();
    void configure_client();
    void retry_pending();
    void cancel_sign_in() noexcept;
    std::string status();
    std::size_t pending_count();
    bool enqueue_backup(const fslib::Path &source, std::string_view user,
                        std::string_view title, std::string_view name);
    void flush_pending(sys::ProgressTask *task = nullptr);

    /// @brief Returns the pointer to the Storage instance.
    remote::Storage *get_remote_storage() noexcept;
} // namespace remote
