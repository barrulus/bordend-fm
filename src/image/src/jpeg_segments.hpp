#pragma once

// Walks the segments of a JPEG before its image data: what nordend::image needs from them
// without decoding (EXIF, size).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace nordend::image::detail {

// The EXIF block of the APP1 "Exif" segment: its TIFF data (after "Exif\0\0") and where that
// data starts in the file. nullopt when there is none before the image data.
struct JpegExif {
    std::span<const std::uint8_t> tiff;
    std::size_t offset{0};
};

[[nodiscard]] std::optional<JpegExif> jpeg_exif(std::span<const std::uint8_t> data);

// Width and height from the start-of-frame segment; nullopt when it is not found.
struct JpegSize {
    std::uint32_t width{0};
    std::uint32_t height{0};
};

[[nodiscard]] std::optional<JpegSize> jpeg_size(std::span<const std::uint8_t> data);

}  // namespace nordend::image::detail
