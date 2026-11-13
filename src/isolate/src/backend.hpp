#pragma once
#include <string_view>

#include "nordend/picture/load.hpp"
#include "nordend/text/text.hpp"

namespace nordend::isolate::backend {
picture::Loaded make_picture(int fd, std::uint32_t width, std::uint32_t height);
picture::PhotoInfo photo_info(int fd);
text::Loaded read_text(int fd, const text::Limits& limits);
void colour(text::Text& value, std::string_view name, std::string_view mime);
void warm_up();
}  // namespace nordend::isolate::backend
