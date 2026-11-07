#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "nordend/image/decode.hpp"

// The one way Nordend turns a file into a picture: thumbnails, Quick Look and the info pane all
// come through load(). The format is told by the file's first bytes, not its name; each format
// takes its own path (decoded whole, or from the preview a camera stored); a file that cannot be
// shown says why in one of a few plain reasons.
namespace nordend::image {

// Why a file gave no picture.
enum class Failure : std::uint8_t {
    none,
    unknown_format,       // not an image format Nordend reads
    no_readable_preview,  // a camera raw file without a preview Nordend can read
    too_large,            // more pixels than DecodeOptions::max_pixels
    damaged,              // the format is known but the data is broken or cut short
    unreadable,           // the file could not be read (permissions, I/O)
};

// The sentence shown for a failure; empty for none.
[[nodiscard]] std::string_view failure_message(Failure failure) noexcept;

struct LoadRequest {
    std::filesystem::path path;
    // The picture fits in max_width x max_height pixels (scaled down, never up).
    std::uint32_t max_width{0};
    std::uint32_t max_height{0};
};

struct Loaded {
    Image image;  // upright, fits in the asked size; empty on failure
    // The size of the whole image, upright, when known (0 when only a preview was read and the
    // file does not say).
    std::uint32_t full_width{0};
    std::uint32_t full_height{0};
    bool from_preview{false};  // made from a preview stored in the file, not the whole image
    Failure failure{Failure::none};
    std::string detail;  // the technical reason of a failure, for the log

    [[nodiscard]] std::string_view error() const noexcept { return failure_message(failure); }
};

// The picture of request.path: from a preview the file carries when that is at least as large as
// asked (camera JPEGs, raw files), else from the whole image; a raw file without one large enough
// gives the largest it has. Scaled down to fit, then turned upright. Blocking: call it on a
// worker. Never throws.
[[nodiscard]] Loaded load(const LoadRequest& request);

// Whether a file of this MIME type may give a picture: the image formats Nordend decodes and
// camera raw files. Used to choose between a thumbnail and an icon before reading anything; the
// file's bytes decide in load().
[[nodiscard]] bool may_show(std::string_view mime) noexcept;

// How many times load() has asked exiv2 for a file's previews, since the program started (for
// tests: formats that cannot carry one never reach it).
[[nodiscard]] std::uint64_t embedded_preview_reads() noexcept;

}  // namespace nordend::image
