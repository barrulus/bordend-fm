#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace nordend::image {

// What a photo says about itself (EXIF), as exiv2 prints it ("1/2000 s", "F5.6"), and the rating
// of its darktable sidecar. Empty strings where the file does not say.
struct PhotoInfo {
    // The image's size upright (the EXIF orientation applied); 0 when unknown.
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::string camera;         // "OM-1", or "Canon EOS 5DS" (the maker added when missing)
    std::string lens;           // "OLYMPUS M.12-100mm F4.0"
    std::string exposure_time;  // "1/2000 s"
    std::string aperture;       // "F5.6"
    std::string iso;            // "200"
    std::string focal_length;   // "25.0 mm"
    std::string taken;          // "2026-08-31 16:34"
    std::string program;        // "Aperture priority"
    std::string exposure_bias;  // "+7/10 EV"
    std::string metering;       // "Spot"
    std::string flash;          // "No, compulsory"
    std::string white_balance;  // "Manual"
    std::string software;       // "Version 1.9"
    // From FILE.xmp (darktable): 1-5 stars, -1 rejected; nullopt without a sidecar or a rating.
    std::optional<int> rating;
};

// Reads the metadata only (no pixels). Never throws: a file exiv2 cannot read gives an empty
// PhotoInfo. Thread-safe.
[[nodiscard]] PhotoInfo read_photo_info(const std::filesystem::path& path);

}  // namespace nordend::image
