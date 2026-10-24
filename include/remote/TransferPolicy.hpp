#pragma once
#include <cstdint>
#include <string_view>

namespace remote
{
    inline bool completed_upload(long code, std::int64_t expected, std::int64_t received, std::string_view id)
    { return (code == 200 || code == 201) && expected > 0 && expected == received && !id.empty(); }

    inline bool retryable_upload(long code)
    { return code == 0 || code == 408 || code == 429 || (code >= 500 && code <= 599); }

    inline bool safe_session_url(std::string_view url)
    { return url.starts_with("https://www.googleapis.com/upload/drive/"); }
}
