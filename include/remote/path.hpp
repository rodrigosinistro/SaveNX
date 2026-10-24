#pragma once

#include "data/TitleInfo.hpp"
#include "data/User.hpp"
#include "remote/Storage.hpp"

namespace remote
{
    /// @brief Moves storage to SaveNX/<stable user key>/<stable title key>.
    /// @param createMissing Creates either folder when it does not exist.
    /// @return true only when both folders exist and storage points at the title folder.
    bool enter_backup_directory(Storage *storage,
                                const data::User *user,
                                const data::TitleInfo *titleInfo,
                                bool createMissing = true);
} // namespace remote
