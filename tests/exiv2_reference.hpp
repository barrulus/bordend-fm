#pragma once

// Nordend's exiv2 code as it was before ROADMAP 2e, kept in the tests as the reference for the
// readers that replace it (see exiv2_reference.cpp).

#include <cstdint>

#include "nordend/formats/image/photo.hpp"
#include "nordend/formats/image/preview.hpp"

namespace nordend::formats::image::reference {

// What embedded_preview(fd, min_long_side) gave through exiv2.
[[nodiscard]] EmbeddedPreview embedded_preview(int fd, std::uint32_t min_long_side);

// What read_photo_info(fd, sidecar_fd) gave through exiv2.
[[nodiscard]] PhotoInfo read_photo_info(int fd, int sidecar_fd);

}  // namespace nordend::formats::image::reference
