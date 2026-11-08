#pragma once

#include <filesystem>

#include "nordend/picture/photo_info.hpp"

namespace nordend::formats::image {

using picture::PhotoInfo;

// Reads the metadata only (no pixels), with Nordend's own reader (camera/info.hpp). Never throws:
// a file it cannot read gives an empty PhotoInfo. Thread-safe.
[[nodiscard]] PhotoInfo read_photo_info(const std::filesystem::path& path);

// The same from a file open as fd and its sidecar as sidecar_fd (-1: none), both the caller's.
[[nodiscard]] PhotoInfo read_photo_info(int fd, int sidecar_fd);

}  // namespace nordend::formats::image
