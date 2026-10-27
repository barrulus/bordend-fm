#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nordend::image {

// A JPEG that a camera file carries next to its main image: the EXIF thumbnail (160x120), the
// maker-note preview of Olympus / OM System JPEGs (1600x1200) and raw files (3200x2400), and the
// previews exiv2 knows in other raw formats. Decoding one is a fraction of decoding the photo.
struct EmbeddedPreview {
    std::vector<std::uint8_t> jpeg;  // empty when there is none
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint8_t orientation{1};  // the main image's EXIF orientation, which the preview shares
    std::string error;            // the file could not be read; empty when it simply has none
};

// The smallest embedded preview whose long side is at least min_long_side, else the largest
// one. Reads only the metadata and the preview's bytes. Thread-safe.
[[nodiscard]] EmbeddedPreview embedded_preview(const std::filesystem::path& path,
                                               std::uint32_t min_long_side);

}  // namespace nordend::image
