#include "nordend/fs/folder_icon.hpp"

#include <sys/stat.h>

#include <doctest/doctest.h>

#include "nordend/fs/directory.hpp"
#include "temp_dir.hpp"

using nordend::fs::FolderIcons;
using nordend::fs::parse_folder_icon;
using nordend::test::TempDir;
namespace fs = std::filesystem;

TEST_CASE("folder icons: Desktop Entry group, escapes and local paths") {
    CHECK(parse_folder_icon("[Desktop Entry]\nIcon=folder-development\n", "/work") ==
          "folder-development");
    CHECK(parse_folder_icon("Icon=wrong\n[Other]\nIcon=wrong\n", "/work").empty());
    CHECK(parse_folder_icon(" # comment\r\n[Desktop Entry]\r\n Icon = folder-music \r\n"
                            "[Other]\nIcon=wrong\n",
                            "/work") == "folder-music");
    CHECK(parse_folder_icon("[Desktop Entry]\nIcon=./my\\sicon.png\n", "/work") ==
          "/work/my icon.png");
    CHECK(parse_folder_icon("[Desktop Entry]\nIcon=/icons/custom.svg\n", "/work") ==
          "/icons/custom.svg");
    CHECK(parse_folder_icon("[Desktop Entry]\nIcon=folder\\q\n", "/work").empty());
    CHECK(parse_folder_icon("[Desktop Entry]\nIcon=folder\\\n", "/work").empty());
    CHECK(
        parse_folder_icon("[Desktop Entry]\nIcon=https://example.org/icon.svg\n", "/work").empty());
    CHECK(parse_folder_icon("[Desktop Entry]\nIcon=../icon.svg\n", "/work").empty());
    CHECK(parse_folder_icon("[Desktop Entry]\nIcon=\n", "/work").empty());
}

TEST_CASE("folder icons: XDG paths, disabled directories and overrides") {
    TempDir dir;
    const auto home = dir.path / "home";
    fs::create_directories(home / "Musik");
    fs::create_directories(dir.path / "config");
    dir.write("config/user-dirs.dirs",
              "XDG_MUSIC_DIR=\"$HOME/Musik\"\n"
              "XDG_DOWNLOAD_DIR=\"$HOME/\"\n");
    const FolderIcons icons{{.home = home, .config_home = dir.path / "config"}};
    CHECK(icons.for_folder(home) == "user-home");
    CHECK(icons.for_folder(home / "Musik/") == "folder-music");
    CHECK(icons.for_folder(home / "Music").empty());
    CHECK(icons.for_folder(home / "Downloads").empty());
    CHECK(icons.for_folder(dir.path / "elsewhere/Musik").empty());
    dir.write("home/Musik/.directory", "[Desktop Entry]\nIcon=folder-red\n");
    CHECK(icons.for_folder(home / "Musik") == "folder-red");
    dir.write("home/Musik/.directory", "[Other]\nIcon=wrong\n");
    CHECK(icons.for_folder(home / "Musik") == "folder-music");
}

TEST_CASE("folder icons: standard defaults only when user-dirs config is absent") {
    TempDir dir;
    const FolderIcons icons{{.home = dir.path, .config_home = dir.path}};
    CHECK(icons.for_folder(dir.path / "Desktop") == "user-desktop");
    CHECK(icons.for_folder(dir.path / "Documents") == "folder-documents");
    CHECK(icons.for_folder(dir.path / "Downloads") == "folder-download");
    CHECK(icons.for_folder(dir.path / "Music") == "folder-music");
    CHECK(icons.for_folder(dir.path / "Pictures") == "folder-pictures");
    CHECK(icons.for_folder(dir.path / "Videos") == "folder-videos");
    CHECK(icons.for_folder(dir.path / "Public") == "folder-publicshare");
    CHECK(icons.for_folder(dir.path / "Templates") == "folder-templates");
    dir.write("user-dirs.dirs", "");
    const FolderIcons disabled{{.home = dir.path, .config_home = dir.path}};
    CHECK(disabled.for_folder(dir.path / "Music").empty());
}

TEST_CASE("folder icons: malformed, oversized and non-regular metadata is ignored") {
    TempDir dir;
    const FolderIcons icons{{.home = dir.path / "home", .config_home = dir.path}};
    CHECK(icons.for_folder(dir.path).empty());
    dir.write(".directory", std::string(65537, 'x'));
    CHECK(icons.for_folder(dir.path).empty());
    fs::remove(dir.path / ".directory");
    REQUIRE(::mkfifo((dir.path / ".directory").c_str(), 0600) == 0);
    CHECK(icons.for_folder(dir.path).empty());
    fs::remove(dir.path / ".directory");
    fs::create_directory(dir.path / ".directory");
    CHECK(icons.for_folder(dir.path).empty());
}

TEST_CASE("folder icons: directory loads and incremental reads carry icons, including symlinks") {
    TempDir dir;
    fs::create_directory(dir.path / "custom");
    dir.write("custom/.directory", "[Desktop Entry]\nIcon=folder-development\n");
    fs::create_directory_symlink("custom", dir.path / "link");
    dir.write("plain.txt", "text");
    std::vector<nordend::fs::Entry> entries;
    const auto result = nordend::fs::read_directory(dir.path, {}, {}, [&](auto chunk) {
        entries.insert(entries.end(), chunk.begin(), chunk.end());
    });
    REQUIRE_FALSE(result.error);
    REQUIRE(entries.size() == 3);
    for (const auto& entry : entries) {
        CHECK(entry.folder_icon == (entry.is_directory() ? "folder-development" : ""));
    }
    dir.write("custom/.directory", "[Desktop Entry]\nIcon=folder-blue\n");
    const auto updated = nordend::fs::read_entries(dir.path, {"custom", "link"}, {});
    REQUIRE(updated[0]);
    REQUIRE(updated[1]);
    CHECK(updated[0]->folder_icon == "folder-blue");
    CHECK(updated[1]->folder_icon == "folder-blue");
    CHECK(updated[1]->is_link);
}
