#define DOCTEST_CONFIG_IMPLEMENT
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>
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
    // Nor the user's configuration, data or cache: a key that writes there (Ctrl+D adds a
    // bookmark to gtk-3.0/bookmarks) writes into the test root. Set before any thread starts.
    for (const auto& [name, folder] :
         {std::pair{"XDG_CONFIG_HOME", "config"}, std::pair{"XDG_DATA_HOME", "data"},
          std::pair{"XDG_CACHE_HOME", "cache"}}) {
        // NOLINTNEXTLINE(concurrency-mt-unsafe): no other thread yet
        setenv(name, (std::filesystem::path{root} / folder).c_str(), 1);
    }
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    const int result = context.run();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return result;
}
