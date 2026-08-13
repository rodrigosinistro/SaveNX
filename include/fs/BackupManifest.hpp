#pragma once

#include "data/TitleInfo.hpp"
#include "data/User.hpp"
#include "fs/MiniZip.hpp"

#include <string_view>
#include <switch.h>

namespace fs
{
    inline constexpr std::string_view NAME_BACKUP_MANIFEST = "savenx_manifest.json";

    /// @brief Writes a human-readable, versioned description of the backup into the ZIP.
    bool write_backup_manifest(MiniZip &zip,
                               const data::User *user,
                               const data::TitleInfo *titleInfo,
                               const FsSaveDataInfo *saveInfo);
} // namespace fs
