#include "remote/path.hpp"

#include "stringutil.hpp"

namespace
{
    bool enter_directory(remote::Storage *storage, std::string_view name, bool createMissing)
    {
        const bool exists = storage->directory_exists(name);
        const bool created = !exists && createMissing && storage->create_directory(name);
        if (!exists && !created) { return false; }

        remote::Item *directory = storage->get_directory_by_name(name);
        if (!directory) { return false; }

        storage->change_directory(directory);
        return true;
    }
} // namespace

bool remote::enter_backup_directory(Storage *storage,
                                    const data::User *user,
                                    const data::TitleInfo *titleInfo,
                                    bool createMissing)
{
    if (!storage || !user || !titleInfo) { return false; }

    storage->return_to_root();

    const std::string userKey = user->get_storage_key();
    if (!enter_directory(storage, userKey, createMissing))
    {
        storage->return_to_root();
        return false;
    }

    const std::string titleKey =
        stringutil::get_formatted_string("TITLE_%016llX", titleInfo->get_application_id());
    if (!enter_directory(storage, titleKey, createMissing))
    {
        storage->return_to_root();
        return false;
    }

    return true;
}
