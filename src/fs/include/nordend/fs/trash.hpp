#pragma once

#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>

#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/transfer.hpp"

// The trash, as the freedesktop Trash specification 1.0 has it: what Delete does. Blocking: run
// it on the transfer thread (TransferQueue does, for TransferKind::trash).
namespace nordend::fs {

// The home trash: $XDG_DATA_HOME/Trash, else ~/.local/share/Trash; empty without a home.
[[nodiscard]] std::filesystem::path home_trash();

// The top directory of path's file system: its highest ancestor on the same device (the mount
// point). path must exist; it is made absolute and its links resolved first.
[[nodiscard]] std::filesystem::path mount_top(const std::filesystem::path& path);

struct TrashDir {
    std::filesystem::path path;  // holds files/ and info/
    // Paths in its .trashinfo files are relative to this; empty for the home trash (absolute).
    std::filesystem::path relative_to;
};

// The user's trash in topdir (a mount point): topdir/.Trash/uid when topdir/.Trash is a real
// folder (not a link) with the sticky bit, else topdir/.Trash-uid; made when missing, refused
// when it is not a folder owned by uid. Its files/ and info/ are made too. nullopt (and error) when
// none can be used.
[[nodiscard]] std::optional<TrashDir> top_trash(const std::filesystem::path& topdir, uid_t uid,
                                                std::error_code& error);

// "photo.ORF" as RFC 2396 escapes it, '/' kept when keep_slash: what .trashinfo files and
// directorysizes hold (GLib's g_uri_escape_string).
[[nodiscard]] std::string escape_path(std::string_view path, bool keep_slash);

// The contents of a .trashinfo file: original (absolute, or relative to the trash's top
// directory), deleted at when in local time.
[[nodiscard]] std::string trash_info(const std::filesystem::path& original, std::time_t when);

// The name the id-th copy of name takes in a trash, as GLib numbers them: "P1.ORF", "P1.2.ORF",
// "P1.3.ORF"; "Trip", "Trip.2".
[[nodiscard]] std::string trash_name(std::string_view name, int id);

// Moves request.names from request.source to the trash; request.destination is the home trash
// (home_trash(), or a test's). Each item goes to the trash of its file system: the home trash on
// the home trash's one, else its top directory's (top_trash); never copied between file systems:
// an item that has no trash there stays and is an error. A regular file takes its darktable
// sidecars along, each with its own .trashinfo. Folders are listed in directorysizes. The trash
// and what it holds are not trashed again.
//
// The result: placed = the names trashed (not their sidecars), files_done = items trashed,
// sidecars included; errors as run_transfer's.
[[nodiscard]] TransferResult run_trash(const TransferRequest& request,
                                       const core::CancelToken& token,
                                       const TransferCallbacks& callbacks);

}  // namespace nordend::fs
