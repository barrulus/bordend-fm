#pragma once

namespace nordend::image::detail {

// Readies exiv2 once per process (thread-safe XMP parsing, no log output). Call it before any
// exiv2 use.
void init_exiv2();

}  // namespace nordend::image::detail
