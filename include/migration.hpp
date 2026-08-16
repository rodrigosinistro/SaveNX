#pragma once

namespace migration
{
    /// @brief Migrates SaveNX v0.1.0 data into sdmc:/switch/SaveNX without overwriting existing files.
    /// @return True when every legacy item was moved or did not need migration.
    bool migrate_v0_1_0_layout();
} // namespace migration
