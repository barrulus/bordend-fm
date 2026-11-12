#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nordend/fs/places.hpp"

namespace nordend::fs {

// KDE's [Desktop Entry] Icon from a .directory file. Relative ./ paths are resolved
// against the folder; an empty result means no override.
[[nodiscard]] std::string parse_folder_icon(std::string_view text,
                                            const std::filesystem::path& folder);

// Blocking: construct once per listing on a worker, then resolve each directory.
// Explicit .directory icons take precedence over XDG special-folder icons.
class FolderIcons {
public:
    explicit FolderIcons(const PlaceSources& sources = PlaceSources::from_environment());
    [[nodiscard]] std::string for_folder(const std::filesystem::path& folder) const;

private:
    std::vector<std::pair<std::filesystem::path, std::string>> standard_;
};

}  // namespace nordend::fs
