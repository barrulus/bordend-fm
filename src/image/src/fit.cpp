#include "nordend/image/fit.hpp"

#include <algorithm>
#include <utility>

#include "nordend/image/preview.hpp"
#include "nordend/image/scale.hpp"

namespace nordend::image {

Fitted load_to_fit(const std::filesystem::path& file, std::uint32_t max_width,
                   std::uint32_t max_height) {
    Fitted result;
    const std::uint32_t side = std::max(max_width, max_height);
    const EmbeddedPreview preview = embedded_preview(file, side);
    Image picture;
    std::uint8_t orientation = 1;
    const auto from_preview = [&] {
        Decoded decoded = decode(preview.jpeg, {.apply_orientation = false});
        if (!decoded.error.empty()) {
            return false;
        }
        picture = std::move(decoded.image);
        orientation = preview.orientation;
        result.from_preview = true;
        return true;
    };
    if (!preview.jpeg.empty() && std::max(preview.width, preview.height) >= side) {
        from_preview();
    }
    if (picture.empty()) {
        Decoded decoded = decode_file(file, {.apply_orientation = false});
        if (decoded.error.empty()) {
            picture = std::move(decoded.image);
            orientation = decoded.orientation;
            result.full_width = picture.width;
            result.full_height = picture.height;
        } else if (preview.jpeg.empty() || !from_preview()) {
            result.error = decoded.error;
            return result;
        }
    }
    if (result.full_width == 0 && preview.full_width > 0) {
        result.full_width = preview.full_width;
        result.full_height = preview.full_height;
    }
    if (orientation >= 5) {
        std::swap(result.full_width, result.full_height);
    }
    // Scaled first, turned after: the turn then works on screen-sized pixels.
    result.image = apply_orientation(scale_to_fit(picture, max_width, max_height), orientation);
    return result;
}

}  // namespace nordend::image
