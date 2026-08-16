#include "app_paths.hpp"
#include "oauth_client.hpp"

#include <array>
#include <cassert>
#include <string_view>

int main()
{
    using namespace savenx::paths;

    constexpr std::array<std::string_view, 14> writablePaths = {BACKUP_DIR,
                                                                 CONFIG_DIR,
                                                                 CACHE_DIR,
                                                                 LOG_DIR,
                                                                 TEMP_DIR,
                                                                 CONFIG_FILE,
                                                                 CUSTOM_PATHS_FILE,
                                                                 GOOGLE_DRIVE_FILE,
                                                                 WEBDAV_FILE,
                                                                 CACHE_FILE,
                                                                 LOG_FILE,
                                                                 TEMP_BACKUP_FILE,
                                                                 TEMP_PATCH_FILE,
                                                                 TEMP_DOWNLOAD_FILE};

    for (const std::string_view path : writablePaths)
    {
        assert(path.starts_with(APP_ROOT));
        assert(path != APP_ROOT);
    }

    const bool hasClient = savenx::oauth::has_embedded_google_client();
    assert(hasClient == (!savenx::oauth::GOOGLE_CLIENT_ID.empty() &&
                         !savenx::oauth::GOOGLE_CLIENT_SECRET.empty()));
    return 0;
}
