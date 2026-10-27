#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "nordend/image/decode.hpp"

namespace nordend::image {

struct Fitted {
    Image image;  // upright, fits in the asked size; empty on failure
    // The size of the whole image, upright, when known (0 when only a preview was read and the
    // file does not say).
    std::uint32_t full_width{0};
    std::uint32_t full_height{0};
    bool from_preview{false};
    std::string error;
};

// The image of file to show in max_width x max_height pixels: from a preview it carries when that
// is at least as large as needed (or when the file cannot be decoded at all: raw files), else
// from the whole image; scaled down to fit, then turned upright. Blocking: call it on a worker.
[[nodiscard]] Fitted load_to_fit(const std::filesystem::path& file, std::uint32_t max_width,
                                 std::uint32_t max_height);

}  // namespace nordend::image
