#pragma once

// Reads TIFF structures: EXIF blocks, the IFD0 of TIFF-based raw files (whatever their magic
// number: 42, RW2's 0x55, ORF's "RO"), and the small uncompressed previews some raw files carry.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "nordend/formats/image/decode.hpp"

namespace nordend::formats::image::detail {

// TIFF integers in the byte order the data declares. Offsets count from the start of data.
class TiffReader {
public:
    // nullopt unless data starts with "II" or "MM" and the offset of a first IFD.
    [[nodiscard]] static std::optional<TiffReader> open(std::span<const std::uint8_t> data);

    [[nodiscard]] std::size_t first_ifd() const { return u32(4); }

    [[nodiscard]] bool has(std::size_t offset, std::size_t size) const {
        return offset <= data_.size() && size <= data_.size() - offset;
    }

    [[nodiscard]] std::uint16_t u16(std::size_t offset) const {
        const auto a = data_[offset];
        const auto b = data_[offset + 1];
        return static_cast<std::uint16_t>(little_ ? a | (b << 8) : (a << 8) | b);
    }

    [[nodiscard]] std::uint32_t u32(std::size_t offset) const {
        const std::uint32_t high = u16(offset + (little_ ? 2 : 0));
        const std::uint32_t low = u16(offset + (little_ ? 0 : 2));
        return (high << 16) | low;
    }

    // The values of a SHORT or LONG tag in the IFD at ifd; empty when it is missing, of another
    // type, or points outside the data.
    [[nodiscard]] std::vector<std::uint32_t> numbers(std::size_t ifd, std::uint16_t tag) const;

    // The first of them.
    [[nodiscard]] std::optional<std::uint32_t> number(std::size_t ifd, std::uint16_t tag) const;

private:
    TiffReader(std::span<const std::uint8_t> data, bool little) : data_{data}, little_{little} {}

    std::span<const std::uint8_t> data_;
    bool little_;
};

// The JPEG inside the first IFD of a TIFF compressed as JPEG in one strip (compression 7, or
// the old 6 when the strip is a whole JPEG): DNG previews are stored so (Leica M240's thumbnail,
// Samsung GX20's full-size preview). nullopt for anything else.
[[nodiscard]] std::optional<std::span<const std::uint8_t>> jpeg_in_tiff(
    std::span<const std::uint8_t> data);

// Decodes the first IFD of a TIFF when it is the simple kind raw files carry as previews:
// uncompressed, chunky, 8-bit RGB, in strips. Anything else fails with a reason.
void decode_tiff_rgb8(std::span<const std::uint8_t> data, const DecodeOptions& options,
                      Decoded& result);

}  // namespace nordend::formats::image::detail
