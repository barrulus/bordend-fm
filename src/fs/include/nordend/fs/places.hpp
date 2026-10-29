#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/file_watcher.hpp"

namespace nordend::fs {

// A place the sidebar lists: a folder worth one click.
struct Place {
    enum class Section : std::uint8_t { places, bookmarks, devices };

    Section section{Section::places};
    std::string label;
    std::filesystem::path path;
    std::string_view icon;  // a Tabler icon name

    bool operator==(const Place&) const = default;
};

// Where the places come from: the home, the XDG config dir and the mount table.
struct PlaceSources {
    std::filesystem::path home;
    std::filesystem::path config_home;  // user-dirs.dirs and gtk-3.0/bookmarks are in it
    std::filesystem::path mountinfo{"/proc/self/mountinfo"};

    // $HOME, $XDG_CONFIG_HOME (or ~/.config).
    [[nodiscard]] static PlaceSources from_environment();
};

// The XDG user dirs in user-dirs.dirs's text ("XDG_PICTURES_DIR=\"$HOME/Pictures\""), by their
// key without XDG_ and _DIR ("PICTURES"), in file order. A dir that is the home itself is left
// out: the spec reads it as disabled.
struct UserDir {
    std::string key;
    std::filesystem::path path;

    bool operator==(const UserDir&) const = default;
};

[[nodiscard]] std::vector<UserDir> parse_user_dirs(std::string_view text,
                                                   const std::filesystem::path& home);

// A /proc file whole (/proc/self/mountinfo): they report a size of 0, so core::read_text_file
// reads nothing; this reads until the end. Empty when it cannot be opened.
[[nodiscard]] std::string read_proc_file(const std::filesystem::path& path);

// The folder of templates "New File" offers: XDG_TEMPLATES_DIR from user-dirs.dirs, or ~/Templates
// without that file (as the sidebar's user dirs); empty when there is none.
[[nodiscard]] std::filesystem::path templates_dir(const PlaceSources& sources);

// The local folders in a GTK bookmarks file ("file:///path%20with%20spaces Label"); other URIs
// (sftp://, smb://) are left out. Without a label, the folder's name.
[[nodiscard]] std::vector<Place> parse_bookmarks(std::string_view text);

// The mount points in /proc/self/mountinfo's text under /run/media, /media and /mnt: where
// removable and data disks land. Octal escapes (\040 for a space) decoded.
[[nodiscard]] std::vector<Place> parse_mounts(std::string_view text);

// Every place, in the order the sidebar shows them: home, the user dirs that exist (Desktop,
// Documents, Downloads, Music, Pictures, Videos; without user-dirs.dirs, the folders with those
// names in the home), the file system, the bookmarks, the mounts.
[[nodiscard]] std::vector<Place> load_places(const PlaceSources& sources);

// The places, kept current: reloaded when something is mounted or unmounted (the mount table
// polls as a priority event) and when user-dirs.dirs or the bookmarks change.
//
// Loop thread only. Must not outlive the loop.
class PlacesWatcher {
public:
    PlacesWatcher(core::EventLoop& loop, PlaceSources sources, std::function<void()> changed);
    ~PlacesWatcher();

    PlacesWatcher(const PlacesWatcher&) = delete;
    PlacesWatcher& operator=(const PlacesWatcher&) = delete;
    PlacesWatcher(PlacesWatcher&&) = delete;
    PlacesWatcher& operator=(PlacesWatcher&&) = delete;

    [[nodiscard]] const std::vector<Place>& places() const noexcept { return places_; }

private:
    void reload();

    core::EventLoop& loop_;
    PlaceSources sources_;
    std::function<void()> changed_;
    std::vector<Place> places_;
    int mountinfo_fd_{-1};
    core::EventLoop::SourceId mountinfo_source_{0};
    std::vector<std::unique_ptr<core::FileWatcher>> files_;
};

}  // namespace nordend::fs
