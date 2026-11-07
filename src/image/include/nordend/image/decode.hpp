#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nordend::image {

// Decoded pixels: RGBA, 8 bits per channel, straight (not premultiplied) alpha, rows top to
// bottom with no padding.
struct Image {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba;

    [[nodiscard]] bool empty() const noexcept { return width == 0 || height == 0; }
};

enum class Format : std::uint8_t { unknown, png, jpeg, gif, webp, jxl, pnm };

[[nodiscard]] std::string_view format_name(Format format) noexcept;

// The format from the first bytes of a file (16 are enough); unknown when none matches.
[[nodiscard]] Format sniff(std::span<const std::uint8_t> head) noexcept;

struct DecodeOptions {
    // Larger images are refused before any pixel memory is allocated (200 MP: 800 MB of RGBA).
    std::uint64_t max_pixels{200'000'000};
    // Turn the pixels as the EXIF orientation says, so that the image stands as it was shot.
    bool apply_orientation{true};
};

struct Decoded {
    Image image;  // empty on failure
    Format format{Format::unknown};
    std::uint8_t orientation{1};  // EXIF orientation found in the file, 1-8 (1: as stored)
    std::string error;            // why it failed; empty on success
};

// Decodes a PNG, JPEG, GIF (first frame), WebP (first frame) or JPEG XL image. Never throws for
// bad input: problems land in error. Thread-safe: call it from workers.
[[nodiscard]] Decoded decode(std::span<const std::uint8_t> data, const DecodeOptions& options = {});

// Reads the file and decodes it.
[[nodiscard]] Decoded decode_file(const std::filesystem::path& path,
                                  const DecodeOptions& options = {});

// The EXIF orientation (1-8) in a TIFF-structured EXIF block, with or without the "Exif\0\0"
// prefix of a JPEG APP1 segment; 1 when absent or unreadable.
[[nodiscard]] std::uint8_t exif_orientation(std::span<const std::uint8_t> exif) noexcept;

// The image turned as EXIF orientation says (2-8 mirror and/or rotate; 1 or anything else
// returns it as it is).
[[nodiscard]] Image apply_orientation(Image image, std::uint8_t orientation);

}  // namespace nordend::image
