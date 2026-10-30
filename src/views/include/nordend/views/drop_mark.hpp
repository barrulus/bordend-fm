#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "nordend/fs/entry.hpp"
#include "nordend/views/folder.hpp"

namespace nordend::views {

// What a view shows at a window point: the folder shown there and the entry under it, if any
// (where a drag starts, where a drop goes).
struct ViewHit {
    std::shared_ptr<Folder> folder;
    std::optional<std::size_t> entry;
};

// Where a drag over the window would drop, as the views draw it: the entry called name in
// directory (a folder), or without a name the view (or column) of directory itself.
struct DropMark {
    std::filesystem::path directory;  // empty: nowhere
    std::string name;

    [[nodiscard]] bool holds(const std::filesystem::path& folder, const fs::Entry& entry) const {
        return !name.empty() && folder == directory && entry.name == name;
    }

    // The whole view of folder is where it goes.
    [[nodiscard]] bool covers(const std::filesystem::path& folder) const {
        return name.empty() && !directory.empty() && folder == directory;
    }
};

}  // namespace nordend::views
