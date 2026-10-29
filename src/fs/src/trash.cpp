#include "nordend/fs/trash.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <format>
#include <fstream>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "nordend/fs/operations.hpp"

namespace nordend::fs {
namespace {

constexpr std::string_view kInfoSuffix = ".trashinfo";
constexpr std::string_view kSizes = "directorysizes";

std::error_code last_error() { return {errno, std::generic_category()}; }

std::optional<struct stat> status_of(const std::filesystem::path& path) {
    struct stat info{};
    if (lstat(path.c_str(), &info) != 0) {
        return std::nullopt;
    }
    return info;
}

// mkdir that is fine with a folder already there.
std::error_code make_directory(const std::filesystem::path& path) {
    if (mkdir(path.c_str(), 0700) == 0 || errno == EEXIST) {
        return {};
    }
    return last_error();
}

// mkdir -p, the folders made private (the home trash and, when missing, its parents).
// NOLINTNEXTLINE(misc-no-recursion): as deep as the path
std::error_code make_directories(const std::filesystem::path& path) {
    if (status_of(path)) {
        return {};
    }
    if (path.has_parent_path() && path.parent_path() != path) {
        if (const std::error_code error = make_directories(path.parent_path())) {
            return error;
        }
    }
    return make_directory(path);
}

std::error_code rename_new(const std::filesystem::path& from, const std::filesystem::path& to) {
    if (renameat2(AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), RENAME_NOREPLACE) == 0) {
        return {};
    }
    if (errno != EINVAL && errno != ENOSYS) {
        return last_error();
    }
    if (status_of(to)) {
        return std::make_error_code(std::errc::file_exists);
    }
    return std::rename(from.c_str(), to.c_str()) == 0 ? std::error_code{} : last_error();
}

// Writes all of text to fd.
std::error_code write_all(int fd, std::string_view text) {
    while (!text.empty()) {
        const ssize_t written = write(fd, text.data(), text.size());
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return last_error();
        }
        text.remove_prefix(static_cast<std::size_t>(written));
    }
    return {};
}

std::string unescape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    const auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const int high = hex(text[i + 1]);
            const int low = hex(text[i + 2]);
            if (high >= 0 && low >= 0) {
                out.push_back(static_cast<char>((high << 4U) | low));
                i += 2;
                continue;
            }
        }
        out.push_back(text[i]);
    }
    return out;
}

// What du -B1 says: the blocks of path and everything in it, a hard link counted once.
std::uint64_t disk_usage(const std::filesystem::path& path) {
    std::uint64_t bytes = 0;
    std::set<std::pair<dev_t, ino_t>> linked;
    const auto add = [&](const struct stat& info) {
        if (info.st_nlink > 1 && !S_ISDIR(info.st_mode) &&
            !linked.emplace(info.st_dev, info.st_ino).second) {
            return;
        }
        bytes += static_cast<std::uint64_t>(info.st_blocks) * 512U;
    };
    if (const std::optional<struct stat> info = status_of(path)) {
        add(*info);
    }
    std::error_code ignored;
    for (std::filesystem::recursive_directory_iterator it{path, ignored}, end; it != end;
         it.increment(ignored)) {
        if (const std::optional<struct stat> info = status_of(it->path())) {
            add(*info);
        }
    }
    return bytes;
}

// A trashed folder, for directorysizes.
struct SizedFolder {
    std::string name;  // in files/
    std::uint64_t bytes{0};
    std::int64_t info_mtime{0};
};

// Adds folders to trash/directorysizes: through a temporary renamed over it, as the
// specification asks; lines of folders no longer in files/ are dropped.
void add_directory_sizes(const std::filesystem::path& trash,
                         const std::vector<SizedFolder>& folders) {
    std::string text;
    {
        std::ifstream in{trash / kSizes};
        std::string line;
        while (std::getline(in, line)) {
            const std::size_t last = line.rfind(' ');
            if (last == std::string::npos) {
                continue;
            }
            const std::string name = unescape(std::string_view{line}.substr(last + 1));
            if (name.empty() || name.find('/') != std::string::npos ||
                !status_of(trash / "files" / name) ||
                std::ranges::any_of(folders,
                                    [&name](const SizedFolder& f) { return f.name == name; })) {
                continue;
            }
            text += line;
            text += '\n';
        }
    }
    for (const SizedFolder& folder : folders) {
        text += std::format("{} {} {}\n", folder.bytes, folder.info_mtime,
                            escape_path(folder.name, false));
    }
    std::string pattern = (trash / std::format("{}.XXXXXX", kSizes)).string();
    const int fd = mkostemp(pattern.data(), O_CLOEXEC);
    if (fd < 0) {
        return;  // a cache: it can be made again later
    }
    const bool written = !write_all(fd, text) && fchmod(fd, 0600) == 0;
    if (close(fd) != 0 || !written || std::rename(pattern.c_str(), (trash / kSizes).c_str()) != 0) {
        unlink(pattern.c_str());
    }
}

// name with bytes dropped from its front (at a UTF-8 character's start) so that extra more fit
// in NAME_MAX; empty when nothing would be left.
std::string shorten_front(const std::string& name, std::size_t extra) {
    if (extra >= NAME_MAX) {
        return {};
    }
    std::size_t cut = name.size() > NAME_MAX - extra ? name.size() - (NAME_MAX - extra) : 0;
    while (cut < name.size() && (static_cast<unsigned char>(name[cut]) & 0xC0U) == 0x80U) {
        ++cut;
    }
    return name.substr(cut);
}

class Trasher {
public:
    Trasher(const TransferRequest& request, const core::CancelToken& token,
            const TransferCallbacks& callbacks)
        : request_{request}, token_{token}, callbacks_{callbacks} {}

    TransferResult run();

private:
    void report() {
        if (callbacks_.progress) {
            callbacks_.progress(progress_);
        }
    }

    void error(const std::string& where, const std::error_code& code) {
        result_.errors.push_back(std::format("{}: {}", where, code.message()));
    }

    // Trashes source/name; false when it stays.
    bool trash_item(const std::string& name);

    // The trash for the items of source, made if needed; nullopt and refused set when none.
    std::optional<TrashDir> find_trash();

    const TransferRequest& request_;
    const core::CancelToken& token_;
    const TransferCallbacks& callbacks_;
    std::filesystem::path source_;  // canonical
    TrashDir trash_;
    std::vector<SizedFolder> folders_;
    TransferResult result_;
    TransferProgress progress_;
};

std::optional<TrashDir> Trasher::find_trash() {
    struct stat folder{};
    if (stat(source_.c_str(), &folder) != 0) {
        result_.refused = std::format("{}: {}", source_.string(), last_error().message());
        return std::nullopt;
    }
    const std::filesystem::path& home = request_.destination;
    if (!home.empty() && request_.trash_top.empty()) {
        if (const std::error_code made = make_directories(home)) {
            result_.refused =
                std::format("Cannot make the trash {}: {}", home.string(), made.message());
            return std::nullopt;
        }
        struct stat there{};
        if (stat(home.c_str(), &there) == 0 && there.st_dev == folder.st_dev) {
            if (const std::error_code made = make_directory(home / "files");
                made || make_directory(home / "info")) {
                result_.refused = std::format("Cannot make the trash {}: {}", home.string(),
                                              (made ? made : last_error()).message());
                return std::nullopt;
            }
            return TrashDir{.path = home, .relative_to = {}};
        }
    }
    const std::filesystem::path top =
        request_.trash_top.empty() ? mount_top(source_) : request_.trash_top;
    std::error_code why;
    std::optional<TrashDir> trash = top_trash(top, geteuid(), why);
    if (!trash) {
        result_.refused = std::format("No trash on the disk of {}: {} cannot hold one ({})",
                                      source_.filename().string(), top.string(), why.message());
    }
    return trash;
}

bool Trasher::trash_item(const std::string& name) {
    const std::filesystem::path item = source_ / name;
    const std::optional<struct stat> info = status_of(item);
    if (!info) {
        error(name, last_error());
        return false;
    }
    std::filesystem::path original = item;
    if (!trash_.relative_to.empty()) {
        // Relative to the top directory, as the specification prefers there.
        original = item.lexically_relative(trash_.relative_to);
        if (original.empty() || *original.begin() == "..") {
            original = item;
        }
    }
    const std::string contents = trash_info(original, std::time(nullptr));

    std::string base = name;
    for (int id = 1;; ++id) {
        std::string candidate = trash_name(base, id);
        if (candidate.size() + kInfoSuffix.size() > NAME_MAX) {
            base = shorten_front(base, candidate.size() - base.size() + kInfoSuffix.size());
            if (base.empty()) {
                error(name, std::make_error_code(std::errc::filename_too_long));
                return false;
            }
            id = 0;
            continue;
        }
        const std::filesystem::path info_file =
            trash_.path / "info" / (candidate + std::string{kInfoSuffix});
        const int fd = open(info_file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0) {
            if (errno == EEXIST) {
                continue;
            }
            error(name, last_error());
            return false;
        }
        // Complete before the item moves: nobody reads an empty one.
        const std::error_code written = write_all(fd, contents);
        if (close(fd) != 0 || written) {
            error(name, written ? written : last_error());
            unlink(info_file.c_str());
            return false;
        }
        const std::error_code moved = rename_new(item, trash_.path / "files" / candidate);
        if (moved == std::errc::file_exists) {
            unlink(info_file.c_str());  // files/ holds one without its info: leave it be
            continue;
        }
        if (moved) {
            unlink(info_file.c_str());
            if (moved == std::errc::cross_device_link) {
                result_.errors.push_back(
                    std::format("{}: on another disk than its trash (a mount point?)", name));
            } else {
                error(name, moved);
            }
            return false;
        }
        if (S_ISDIR(info->st_mode)) {
            struct stat written_info{};
            stat(info_file.c_str(), &written_info);
            folders_.push_back({.name = candidate,
                                .bytes = disk_usage(trash_.path / "files" / candidate),
                                .info_mtime = static_cast<std::int64_t>(written_info.st_mtime)});
        }
        ++progress_.files_done;
        ++result_.files_done;
        report();
        return true;
    }
}

TransferResult Trasher::run() {
    std::error_code canonical;
    source_ = std::filesystem::canonical(request_.source, canonical);
    if (canonical) {
        result_.refused = std::format("{}: {}", request_.source.string(), canonical.message());
        return result_;
    }
    std::optional<TrashDir> trash = find_trash();
    if (!trash) {
        return result_;
    }
    trash_ = std::move(*trash);
    const std::filesystem::path trash_path =
        std::filesystem::weakly_canonical(trash_.path, canonical);
    const auto within = [](const std::filesystem::path& inner, const std::filesystem::path& outer) {
        const auto [end, rest] = std::ranges::mismatch(outer, inner);
        return end == outer.end();
    };
    if (within(source_, trash_path)) {
        result_.refused = "These are in the trash already";
        return result_;
    }

    // The items and their sidecars; a sidecar picked along with its photo goes with it.
    std::vector<std::string> listing;
    {
        std::error_code ignored;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator{source_, ignored}) {
            listing.push_back(entry.path().filename().string());
        }
    }

    struct Group {
        std::string name;
        std::vector<std::string> sidecars;
    };

    std::vector<Group> groups;
    for (const std::string& name : request_.names) {
        const std::optional<struct stat> info = status_of(source_ / name);
        if (!info) {
            error(name, last_error());
            continue;
        }
        if (within(trash_path, source_ / name)) {
            result_.errors.push_back(std::format("{}: it holds the trash", name));
            continue;
        }
        groups.push_back({.name = name,
                          .sidecars = S_ISREG(info->st_mode) ? sidecars_of(listing, name)
                                                             : std::vector<std::string>{}});
    }
    std::erase_if(groups, [&groups](const Group& item) {
        return std::ranges::any_of(groups, [&item](const Group& other) {
            return std::ranges::find(other.sidecars, item.name) != other.sidecars.end();
        });
    });
    for (const Group& group : groups) {
        progress_.files_total += 1 + group.sidecars.size();
    }
    report();

    for (const Group& group : groups) {
        if (token_.cancelled()) {
            result_.cancelled = true;
            break;
        }
        progress_.current = group.name;
        if (!trash_item(group.name)) {
            continue;  // the photo stays: its sidecars stay with it
        }
        result_.placed.push_back(group.name);
        for (const std::string& sidecar : group.sidecars) {
            trash_item(sidecar);
        }
    }
    if (!folders_.empty()) {
        add_directory_sizes(trash_.path, folders_);
    }
    progress_.current.clear();
    report();
    return result_;
}

}  // namespace

std::filesystem::path home_trash() {
    // NOLINTBEGIN(concurrency-mt-unsafe): read on the loop thread, before any setenv
    const char* const data = std::getenv("XDG_DATA_HOME");
    if (data != nullptr && *data == '/') {
        return std::filesystem::path{data} / "Trash";
    }
    const char* const home = std::getenv("HOME");
    // NOLINTEND(concurrency-mt-unsafe)
    if (home != nullptr && *home == '/') {
        return std::filesystem::path{home} / ".local/share/Trash";
    }
    return {};
}

std::filesystem::path mount_top(const std::filesystem::path& path) {
    std::error_code ignored;
    std::filesystem::path top = std::filesystem::canonical(path, ignored);
    if (top.empty()) {
        top = std::filesystem::absolute(path, ignored).lexically_normal();
    }
    struct stat here{};
    if (stat(top.c_str(), &here) != 0) {
        return top;
    }
    while (top.has_parent_path() && top.parent_path() != top) {
        struct stat above{};
        if (stat(top.parent_path().c_str(), &above) != 0 || above.st_dev != here.st_dev) {
            break;
        }
        top = top.parent_path();
    }
    return top;
}

std::optional<TrashDir> top_trash(const std::filesystem::path& topdir, uid_t uid,
                                  std::error_code& error) {
    // A folder of the user's, made when missing.
    const auto usable = [&error, uid](const std::filesystem::path& dir) {
        bool made = false;
        std::optional<struct stat> info = status_of(dir);
        if (!info) {
            if (mkdir(dir.c_str(), 0700) != 0) {
                error = last_error();
                return false;
            }
            made = true;
            info = status_of(dir);
        }
        if (!info || !S_ISDIR(info->st_mode) || info->st_uid != uid) {
            if (made) {
                rmdir(dir.c_str());  // a file system that ignores owners (FAT)
            }
            error = std::make_error_code(std::errc::permission_denied);
            return false;
        }
        return true;
    };
    const auto ready = [&error, &topdir](const std::filesystem::path& dir) {
        std::error_code made = make_directory(dir / "files");
        if (!made) {
            made = make_directory(dir / "info");
        }
        error = made;
        return made ? std::nullopt : std::optional<TrashDir>{{.path = dir, .relative_to = topdir}};
    };

    const std::filesystem::path shared = topdir / ".Trash";
    if (const std::optional<struct stat> info = status_of(shared);
        info && S_ISDIR(info->st_mode) && (info->st_mode & S_ISVTX) != 0) {
        const std::filesystem::path mine = shared / std::to_string(uid);
        if (usable(mine)) {
            return ready(mine);
        }
    }
    const std::filesystem::path mine = topdir / std::format(".Trash-{}", uid);
    if (usable(mine)) {
        return ready(mine);
    }
    return std::nullopt;
}

std::string escape_path(std::string_view path, bool keep_slash) {
    static constexpr std::string_view kDigits = "0123456789ABCDEF";
    std::string out;
    out.reserve(path.size());
    for (const char c : path) {
        const auto byte = static_cast<unsigned char>(c);
        const bool unreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
                                c == '~' || (keep_slash && c == '/');
        if (unreserved) {
            out.push_back(c);
        } else {
            out.push_back('%');
            out.push_back(kDigits[byte >> 4U]);
            out.push_back(kDigits[byte & 0x0FU]);
        }
    }
    return out;
}

std::string trash_info(const std::filesystem::path& original, std::time_t when) {
    std::tm local{};
    localtime_r(&when, &local);
    std::array<char, 32> date{};
    std::strftime(date.data(), date.size(), "%Y-%m-%dT%H:%M:%S", &local);
    return std::format("[Trash Info]\nPath={}\nDeletionDate={}\n",
                       escape_path(original.string(), true), date.data());
}

std::string trash_name(std::string_view name, int id) {
    if (id == 1) {
        return std::string{name};
    }
    const std::size_t dot = name.find('.');
    if (dot == std::string_view::npos) {
        return std::format("{}.{}", name, id);
    }
    return std::format("{}.{}{}", name.substr(0, dot), id, name.substr(dot));
}

TransferResult run_trash(const TransferRequest& request, const core::CancelToken& token,
                         const TransferCallbacks& callbacks) {
    return Trasher{request, token, callbacks}.run();
}

}  // namespace nordend::fs
