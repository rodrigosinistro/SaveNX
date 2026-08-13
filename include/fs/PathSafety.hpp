#pragma once

#include <string_view>

namespace fs
{
    /// @brief Returns true when a ZIP entry is relative and cannot escape the selected save root.
    inline bool zip_path_is_safe(std::string_view path) noexcept
    {
        if (path.empty() || path.front() == '/' || path.front() == '\\' || path.find('\\') != path.npos ||
            path.find(':') != path.npos)
        {
            return false;
        }

        size_t componentBegin{};
        while (componentBegin < path.length())
        {
            const size_t separator = path.find('/', componentBegin);
            const size_t componentEnd = separator == path.npos ? path.length() : separator;
            const std::string_view component = path.substr(componentBegin, componentEnd - componentBegin);
            if (component.empty() || component == "." || component == "..") { return false; }
            if (separator == path.npos || separator + 1 == path.length()) { break; }
            componentBegin = separator + 1;
        }
        return true;
    }
} // namespace fs
