#pragma once

#include <string_view>

// Private builds may generate this file from GitHub Actions secrets. It must never be committed.
#if __has_include("oauth_client.generated.hpp")
#include "oauth_client.generated.hpp"
#else
namespace savenx::oauth
{
    inline constexpr std::string_view GOOGLE_CLIENT_ID{};
    inline constexpr std::string_view GOOGLE_CLIENT_SECRET{};
} // namespace savenx::oauth
#endif

namespace savenx::oauth
{
    inline constexpr bool has_embedded_google_client() noexcept
    {
        return !GOOGLE_CLIENT_ID.empty() && !GOOGLE_CLIENT_SECRET.empty();
    }
} // namespace savenx::oauth
