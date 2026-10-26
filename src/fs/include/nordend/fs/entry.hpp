#pragma once

#include <cstdint>
#include <string>

namespace nordend::fs {

enum class EntryKind : std::uint8_t { file, directory, other, unknown };

// One directory entry, as the views need it. A symbolic link takes the kind of its target
// (a link to a directory is a directory) and keeps is_link; a dangling link is kind other.
struct Entry {
    std::string name;  // bytes as on disk: usually UTF-8, not guaranteed
    EntryKind kind{EntryKind::unknown};
    bool is_link{false};
    std::uint64_t size{0};        // bytes; for a link, its target's
    std::int64_t modified_ns{0};  // mtime, nanoseconds since the epoch

    [[nodiscard]] bool hidden() const noexcept { return !name.empty() && name.front() == '.'; }

    [[nodiscard]] bool is_directory() const noexcept { return kind == EntryKind::directory; }
};

}  // namespace nordend::fs
