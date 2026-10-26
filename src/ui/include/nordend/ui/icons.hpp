#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "nordend/ui/color.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/text.hpp"

namespace nordend::ui {

// Tabler line icons drawn from an icon font by glyph name ("folder", "photo"...), tinted with
// any color. The font is looked up in the XDG data dirs: Nordend's own copy
// (nordend/fonts/tabler-icons.ttf) first, then the one Noctalia installs
// (noctalia/assets/fonts/noctalia-tabler.ttf). Without either, draw() draws nothing and
// available() is false.
//
// Not thread-safe; uses the FontSystem it was made with, which must outlive it.
class IconSet {
public:
    explicit IconSet(FontSystem& fonts);

    [[nodiscard]] bool available() const noexcept { return font_.has_value(); }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    // True when the font has an icon of that name.
    [[nodiscard]] bool has(std::string_view name);

    // Draws the icon centered in the size x size box at (x, y), logical pixels. An unknown name
    // draws the "file" icon, so a typo shows up as a wrong icon rather than a hole.
    void draw(Renderer& renderer, std::string_view name, float x, float y, float size, Color color);

    // Where the icon font is looked for, in order.
    [[nodiscard]] static std::vector<std::filesystem::path> search_paths();

private:
    std::uint32_t glyph(std::string_view name);

    FontSystem& fonts_;
    std::optional<std::uint32_t> font_;
    std::filesystem::path path_;
    std::unordered_map<std::string, std::uint32_t> glyphs_;
};

}  // namespace nordend::ui
