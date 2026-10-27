#include "tiff.hpp"

#include <algorithm>
#include <cstring>

namespace nordend::image::detail {
namespace {

constexpr std::uint16_t kShort = 3;
constexpr std::uint16_t kLong = 4;
constexpr std::size_t kEntrySize = 12;

constexpr std::uint16_t kImageWidth = 256;
constexpr std::uint16_t kImageLength = 257;
constexpr std::uint16_t kBitsPerSample = 258;
constexpr std::uint16_t kCompression = 259;
constexpr std::uint16_t kPhotometric = 262;
constexpr std::uint16_t kStripOffsets = 273;
constexpr std::uint16_t kSamplesPerPixel = 277;
constexpr std::uint16_t kRowsPerStrip = 278;
constexpr std::uint16_t kStripByteCounts = 279;
constexpr std::uint16_t kPlanarConfiguration = 284;

}  // namespace

std::optional<TiffReader> TiffReader::open(std::span<const std::uint8_t> data) {
    if (data.size() < 8) {
        return std::nullopt;
    }
    if (data[0] == 'I' && data[1] == 'I') {
        return TiffReader{data, true};
    }
    if (data[0] == 'M' && data[1] == 'M') {
        return TiffReader{data, false};
    }
    return std::nullopt;
}

std::vector<std::uint32_t> TiffReader::numbers(std::size_t ifd, std::uint16_t tag) const {
    if (!has(ifd, 2)) {
        return {};
    }
    const std::size_t count = u16(ifd);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t entry = ifd + 2 + (i * kEntrySize);
        if (!has(entry, kEntrySize)) {
            return {};
        }
        if (u16(entry) != tag) {
            continue;
        }
        const std::uint16_t type = u16(entry + 2);
        if (type != kShort && type != kLong) {
            return {};
        }
        const std::size_t size = type == kShort ? 2 : 4;
        const std::size_t values = u32(entry + 4);
        if (values == 0 || values > data_.size() / size) {
            return {};
        }
        // Inline when they fit in the four bytes of the entry, else at the offset written there.
        const std::size_t at = values * size <= 4 ? entry + 8 : u32(entry + 8);
        if (!has(at, values * size)) {
            return {};
        }
        std::vector<std::uint32_t> out(values);
        for (std::size_t v = 0; v < values; ++v) {
            out[v] = size == 2 ? u16(at + (v * 2)) : u32(at + (v * 4));
        }
        return out;
    }
    return {};
}

std::optional<std::uint32_t> TiffReader::number(std::size_t ifd, std::uint16_t tag) const {
    const std::vector<std::uint32_t> values = numbers(ifd, tag);
    if (values.empty()) {
        return std::nullopt;
    }
    return values.front();
}

void decode_tiff_rgb8(std::span<const std::uint8_t> data, const DecodeOptions& options,
                      Decoded& result) {
    const std::optional<TiffReader> tiff = TiffReader::open(data);
    if (!tiff) {
        result.error = "not a TIFF";
        return;
    }
    const std::size_t ifd = tiff->first_ifd();
    const std::uint32_t width = tiff->number(ifd, kImageWidth).value_or(0);
    const std::uint32_t height = tiff->number(ifd, kImageLength).value_or(0);
    const std::vector<std::uint32_t> bits = tiff->numbers(ifd, kBitsPerSample);
    const bool rgb8 = tiff->number(ifd, kCompression).value_or(1) == 1 &&
                      tiff->number(ifd, kPhotometric) == 2 &&
                      tiff->number(ifd, kSamplesPerPixel) == 3 &&
                      tiff->number(ifd, kPlanarConfiguration).value_or(1) == 1 &&
                      bits == std::vector<std::uint32_t>{8, 8, 8};
    if (!rgb8) {
        result.error = "a TIFF that is not uncompressed 8-bit RGB";
        return;
    }
    if (width == 0 || height == 0) {
        result.error = "a TIFF without a size";
        return;
    }
    if (std::uint64_t{width} * height > options.max_pixels) {
        result.error = "image too large";
        return;
    }
    const std::uint32_t rows_per_strip = tiff->number(ifd, kRowsPerStrip).value_or(height);
    const std::vector<std::uint32_t> offsets = tiff->numbers(ifd, kStripOffsets);
    const std::vector<std::uint32_t> counts = tiff->numbers(ifd, kStripByteCounts);
    const std::size_t row_bytes = std::size_t{width} * 3;
    if (rows_per_strip == 0 || offsets.empty() || offsets.size() != counts.size()) {
        result.error = "a TIFF without strips";
        return;
    }
    Image& image = result.image;
    image.width = width;
    image.height = height;
    image.rgba.resize(std::size_t{width} * height * 4);
    std::uint32_t y = 0;
    for (std::size_t strip = 0; strip < offsets.size() && y < height; ++strip) {
        const std::uint32_t rows = std::min(rows_per_strip, height - y);
        if (counts[strip] < rows * row_bytes || !tiff->has(offsets[strip], rows * row_bytes)) {
            break;
        }
        const std::uint8_t* from = data.data() + offsets[strip];
        std::uint8_t* to = image.rgba.data() + (std::size_t{y} * width * 4);
        for (std::size_t i = 0; i < std::size_t{rows} * width; ++i, from += 3, to += 4) {
            std::memcpy(to, from, 3);
            to[3] = 255;
        }
        y += rows;
    }
    if (y < height) {
        result.image = {};
        result.error = "a TIFF cut short";
    }
}

}  // namespace nordend::image::detail
