#define DOCTEST_CONFIG_IMPLEMENT
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/fs/trash.hpp"

int main(int argc, char** argv) {
    // No test reads or makes the user's trash: the trash folder lists one of its own, and no
    // disk's.
    std::string root = (std::filesystem::temp_directory_path() / "nordend-tests-XXXXXX").string();
    if (mkdtemp(root.data()) == nullptr) {
        return 1;
    }
    nordend::fs::set_trash_sources({.home = std::filesystem::path{root} / "Trash",
                                    .tops = std::vector<std::filesystem::path>{},
                                    .uid = geteuid()});
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    const int result = context.run();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return result;
}
