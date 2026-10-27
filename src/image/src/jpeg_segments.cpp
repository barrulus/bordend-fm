#include "jpeg_segments.hpp"

#include <algorithm>
#include <array>
#include <functional>

namespace nordend::image::detail {
namespace {

constexpr std::uint8_t kApp1 = 0xE1;         // the EXIF segment
constexpr std::uint8_t kStartOfScan = 0xDA;  // the image data
constexpr std::array<std::uint8_t, 6> kExif{'E', 'x', 'i', 'f', 0, 0};

// Calls visit(marker, payload, payload offset) for each segment from SOI up to the image data,
// until visit returns true. Stops quietly on anything malformed.
void walk(
    std::span<const std::uint8_t> data,
    const std::function<bool(std::uint8_t, std::span<const std::uint8_t>, std::size_t)>& visit) {
    if (data.size() < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return;
    }
    std::size_t at = 2;  // after SOI
    while (at + 4 <= data.size()) {
        if (data[at] != 0xFF) {
            return;  // not a marker
        }
        const std::uint8_t marker = data[at + 1];
        if (marker == 0xFF) {
            ++at;  // fill byte
            continue;
        }
        if (marker == kStartOfScan) {
            return;
        }
        const std::size_t length = (std::size_t{data[at + 2]} << 8) | data[at + 3];
        if (length < 2 || length - 2 > data.size() - (at + 4)) {
            return;
        }
        if (visit(marker, data.subspan(at + 4, length - 2), at + 4)) {
            return;
        }
        at += 2 + length;
    }
}

}  // namespace

std::optional<JpegExif> jpeg_exif(std::span<const std::uint8_t> data) {
    std::optional<JpegExif> exif;
    walk(data, [&](std::uint8_t marker, std::span<const std::uint8_t> payload, std::size_t offset) {
        if (marker != kApp1 || payload.size() < kExif.size() ||
            !std::equal(kExif.begin(), kExif.end(), payload.begin())) {
            return false;
        }
        exif = JpegExif{.tiff = payload.subspan(kExif.size()), .offset = offset + kExif.size()};
        return true;
    });
    return exif;
}

std::optional<JpegSize> jpeg_size(std::span<const std::uint8_t> data) {
    std::optional<JpegSize> size;
    walk(data, [&](std::uint8_t marker, std::span<const std::uint8_t> payload, std::size_t) {
        // SOF0-SOF15, except DHT (C4), JPG (C8) and DAC (CC).
        const bool frame =
            marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (!frame || payload.size() < 5) {
            return false;
        }
        size = JpegSize{.width = (std::uint32_t{payload[3]} << 8) | payload[4],
                        .height = (std::uint32_t{payload[1]} << 8) | payload[2]};
        return true;
    });
    return size;
}

}  // namespace nordend::image::detail
