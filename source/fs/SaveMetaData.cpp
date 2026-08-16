#include "fs/SaveMetaData.hpp"

#include "error.hpp"
#include "fs/directory_functions.hpp"
#include "fs/save_data_functions.hpp"
#include "fs/save_mount.hpp"
#include "fslib.hpp"
#include "logging/logger.hpp"

namespace
{
    constexpr size_t SIZE_EXTRA_DATA = sizeof(FsSaveDataExtraData);
}

bool fs::fill_save_meta_data(const FsSaveDataInfo *saveInfo, fs::SaveMetaData &meta) noexcept
{
    if (!saveInfo) { return false; }

    FsSaveDataExtraData extraData{};
    const bool extraRead = fs::read_save_extra_data(saveInfo, extraData);
    if (extraRead)
    {
        meta = {.magic           = fs::SAVE_META_MAGIC,
                .revision        = 0x01,
                .applicationID   = extraData.attr.application_id,
                .accountID       = extraData.attr.uid,
                .systemSaveID    = extraData.attr.system_save_data_id,
                .saveDataType    = extraData.attr.save_data_type,
                .saveDataRank    = extraData.attr.save_data_rank,
                .saveDataIndex   = extraData.attr.save_data_index,
                .ownerID         = extraData.owner_id,
                .timestamp       = extraData.timestamp,
                .flags           = extraData.flags,
                .saveDataSize    = extraData.data_size,
                .journalSize     = extraData.journal_size,
                .commitID        = extraData.commit_id,
                .saveDataSpaceID = saveInfo->save_data_space_id};
        return true;
    }

    // SaveNX 0.2.9 lazy discovery intentionally does not enumerate the global save
    // table, therefore save_data_id can legitimately be unknown (zero). That ID is
    // only needed for reading container extra-data; the account/title attributes are
    // still sufficient to mount, copy and restore an existing save. Persist a safe
    // minimal metadata record instead of aborting the backup before the mount occurs.
    meta = {.magic           = fs::SAVE_META_MAGIC,
            .revision        = 0x01,
            .applicationID   = saveInfo->application_id,
            .accountID       = saveInfo->uid,
            .systemSaveID    = saveInfo->system_save_data_id,
            .saveDataType    = saveInfo->save_data_type,
            .saveDataRank    = saveInfo->save_data_rank,
            .saveDataIndex   = saveInfo->save_data_index,
            .ownerID         = 0,
            .timestamp       = 0,
            .flags           = 0,
            .saveDataSize    = 0,
            .journalSize     = 0,
            .commitID        = 0,
            .saveDataSpaceID = saveInfo->save_data_space_id};

    logger::log("Save extra-data unavailable for %016llX; writing attribute-only SaveNX metadata.",
                static_cast<unsigned long long>(saveInfo->application_id));
    return true;
}

bool fs::process_save_meta_data(const FsSaveDataInfo *saveInfo, const SaveMetaData &meta) noexcept
{
    if (!fs::validate_save_meta_data(saveInfo, meta)) { return false; }

    FsSaveDataExtraData extraData{};
    const bool extraRead = fs::read_save_extra_data(saveInfo, extraData);
    if (!extraRead)
    {
        // Attribute-only metadata is generated for lazy candidates whose SaveDataId
        // was intentionally not enumerated. With no container sizes recorded there is
        // nothing to extend, so restoring into an already existing mounted save can
        // safely continue using application/account attributes.
        const bool attributeOnly = meta.saveDataSize == 0 && meta.journalSize == 0;
        if (attributeOnly)
        {
            logger::log("Save extra-data unavailable during restore; using attribute-only metadata.");
            return true;
        }
        return false;
    }

    const bool needsExtend = extraData.data_size < meta.saveDataSize;
    const bool extended    = needsExtend && fs::extend_save_data(saveInfo, meta.saveDataSize, meta.journalSize);
    if (needsExtend && !extended) { return false; }

    return true;
}

bool fs::validate_save_meta_data(const FsSaveDataInfo *saveInfo, const SaveMetaData &meta) noexcept
{
    if (!saveInfo || meta.magic != fs::SAVE_META_MAGIC || meta.revision != 0x01) { return false; }

    const bool sameApplication = meta.applicationID == saveInfo->application_id;
    const bool sameSystemSave  = meta.systemSaveID == saveInfo->system_save_data_id;
    const bool sameType        = meta.saveDataType == saveInfo->save_data_type;
    const bool sameRank        = meta.saveDataRank == saveInfo->save_data_rank;
    const bool sameIndex       = meta.saveDataIndex == saveInfo->save_data_index;
    if (!sameApplication || !sameSystemSave || !sameType || !sameRank || !sameIndex) { return false; }

    const bool saveHasUid = saveInfo->uid.uid[0] != 0 || saveInfo->uid.uid[1] != 0;
    const bool metaHasUid = meta.accountID.uid[0] != 0 || meta.accountID.uid[1] != 0;
    if (saveHasUid || metaHasUid)
    {
        const bool sameUid = meta.accountID.uid[0] == saveInfo->uid.uid[0] && meta.accountID.uid[1] == saveInfo->uid.uid[1];
        if (!sameUid) { return false; }
    }

    return true;
}
