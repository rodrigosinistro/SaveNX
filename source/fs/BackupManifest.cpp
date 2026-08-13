#include "fs/BackupManifest.hpp"

#include "fs/save_data_functions.hpp"
#include "json.hpp"
#include "version.hpp"

#include <cstdio>
#include <ctime>

namespace
{
    std::string hex64(uint64_t value)
    {
        char buffer[17]{};
        std::snprintf(buffer, sizeof(buffer), "%016llX", value);
        return buffer;
    }

    std::string uid_string(AccountUid uid)
    {
        char buffer[33]{};
        std::snprintf(buffer, sizeof(buffer), "%016llX%016llX", uid.uid[0], uid.uid[1]);
        return buffer;
    }

    void add_string(json_object *object, const char *key, std::string_view value)
    {
        json_object_object_add(object, key, json_object_new_string_len(value.data(), static_cast<int>(value.length())));
    }
} // namespace

bool fs::write_backup_manifest(MiniZip &zip,
                               const data::User *user,
                               const data::TitleInfo *titleInfo,
                               const FsSaveDataInfo *saveInfo)
{
    if (!zip.is_open() || !user || !titleInfo || !saveInfo) { return false; }

    FsSaveDataExtraData extraData{};
    const bool hasExtraData = fs::read_save_extra_data(saveInfo, extraData);

    json::Object manifest = json::new_object(json_object_new_object);
    if (!manifest) { return false; }

    add_string(manifest.get(), "schema", "savenx.backup.v1");
    add_string(manifest.get(), "createdBy", "SaveNX");
    add_string(manifest.get(), "appVersion", savenx::VERSION);
    json_object_object_add(manifest.get(), "createdAtUnix", json_object_new_int64(std::time(nullptr)));

    json_object *userJson = json_object_new_object();
    add_string(userJson, "nickname", user->get_nickname());
    add_string(userJson, "storageKey", user->get_storage_key());
    add_string(userJson, "accountUid", uid_string(user->get_account_id()));
    json_object_object_add(userJson, "saveDataType", json_object_new_int(saveInfo->save_data_type));
    json_object_object_add(manifest.get(), "user", userJson);

    json_object *titleJson = json_object_new_object();
    add_string(titleJson, "applicationId", hex64(titleInfo->get_application_id()));
    add_string(titleJson, "name", titleInfo->get_title());
    add_string(titleJson, "publisher", titleInfo->get_publisher());
    json_object_object_add(manifest.get(), "title", titleJson);

    json_object *saveJson = json_object_new_object();
    add_string(saveJson, "saveDataId", hex64(saveInfo->save_data_id));
    add_string(saveJson, "systemSaveDataId", hex64(saveInfo->system_save_data_id));
    json_object_object_add(saveJson, "spaceId", json_object_new_int(saveInfo->save_data_space_id));
    json_object_object_add(saveJson, "type", json_object_new_int(saveInfo->save_data_type));
    json_object_object_add(saveJson, "rank", json_object_new_int(saveInfo->save_data_rank));
    json_object_object_add(saveJson, "index", json_object_new_int(saveInfo->save_data_index));
    json_object_object_add(saveJson, "hasExtraData", json_object_new_boolean(hasExtraData));
    if (hasExtraData)
    {
        add_string(saveJson, "ownerId", hex64(extraData.owner_id));
        add_string(saveJson, "attributeApplicationId", hex64(extraData.attr.application_id));
        add_string(saveJson, "attributeAccountUid", uid_string(extraData.attr.uid));
        json_object_object_add(saveJson, "dataSize", json_object_new_int64(extraData.data_size));
        json_object_object_add(saveJson, "journalSize", json_object_new_int64(extraData.journal_size));
        json_object_object_add(saveJson, "flags", json_object_new_int64(extraData.flags));
        add_string(saveJson, "commitId", hex64(extraData.commit_id));
    }
    json_object_object_add(manifest.get(), "save", saveJson);

    json_object *restoreJson = json_object_new_object();
    json_object_object_add(restoreJson, "containsAllMountedSaveFiles", json_object_new_boolean(true));
    json_object_object_add(restoreJson, "containsContainerMetadata", json_object_new_boolean(hasExtraData));
    json_object_object_add(restoreJson, "preRestoreSafetyBackupRequired", json_object_new_boolean(true));
    json_object_object_add(manifest.get(), "restore", restoreJson);

    const char *manifestString = json_object_to_json_string_ext(manifest.get(), JSON_C_TO_STRING_PRETTY);
    const size_t manifestSize = std::char_traits<char>::length(manifestString);
    const bool opened = zip.open_new_file(fs::NAME_BACKUP_MANIFEST);
    const bool written = opened && zip.write(manifestString, manifestSize);
    const bool closed = opened && zip.close_current_file();
    return opened && written && closed;
}
