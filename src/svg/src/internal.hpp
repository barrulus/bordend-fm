#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace nordend::svg {
inline constexpr std::size_t kMaxBytes = 8U * 1024U * 1024U;
inline constexpr std::size_t kMaxNodes = 100000;
inline constexpr std::size_t kMaxDepth = 256;

struct Failure {};

inline void require(bool condition) {
    if (!condition) {
        throw Failure{};
    }
}

struct Node {
    std::string name;
    std::map<std::string, std::string, std::less<>> attributes;
    std::vector<std::size_t> children;
    std::string text;

    [[nodiscard]] std::string_view attr(std::string_view key) const {
        const auto it = attributes.find(key);
        return it == attributes.end() ? std::string_view{} : it->second;
    }
};

[[nodiscard]] std::vector<Node> xml(std::string_view input);
}  // namespace nordend::svg

#include <array>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "nordend/picture/image.hpp"

namespace nordend::svg {
[[nodiscard]] double number(std::string_view text, double fallback = 0);
using Colour = std::array<double, 4>;
[[nodiscard]] Colour colour(std::string_view text);

struct Canvas {
    picture::Image image;  // Premultiplied until the final conversion.
    std::size_t work{};
    bool exhausted{};
};

struct Raster {
    FT_Library library{};
    Raster();
    ~Raster();
    Raster(const Raster&) = delete;
    Raster& operator=(const Raster&) = delete;
    void rectangle(Canvas& canvas, double x, double y, double width, double height, Colour paint);
};

void composite(Canvas& canvas, int x, int y, int length, unsigned coverage, Colour paint) noexcept;
void straighten(picture::Image& image);
}  // namespace nordend::svg
