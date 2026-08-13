#include "fs/PathSafety.hpp"

#include <array>
#include <cassert>
#include <string_view>

int main()
{
    constexpr std::array<std::string_view, 7> safePaths = {
        "progress.dat", "slot/01/save.bin", "unicode/ação.bin", ".settings", "dir/", "a/b/c", "space name/file"};
    constexpr std::array<std::string_view, 10> unsafePaths = {
        "", "/absolute", "\\absolute", "../escape", "dir/../escape", "./file", "dir//file", "C:/file", "save:\\file", "dir\\file"};

    for (const std::string_view path : safePaths) { assert(fs::zip_path_is_safe(path)); }
    for (const std::string_view path : unsafePaths) { assert(!fs::zip_path_is_safe(path)); }
    return 0;
}
