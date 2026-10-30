#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "nordend/ui/color.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/text.hpp"

namespace nordend::ui {

// Tabler line icons drawn from an icon font by glyph name ("folder", "photo"...), tinted with
// any color. The fonts are compiled into the program (third_party/tabler-icons, only the icons
// Nordend uses): the outline icons, and the filled ones asked for with a "-filled" suffix
// ("trash-filled"). Without FreeType able to load them, draw() draws nothing and available() is
// false.
//
// Not thread-safe; uses the FontSystem it was made with, which must outlive it.
class IconSet {
public:
    explicit IconSet(FontSystem& fonts);

    [[nodiscard]] bool available() const noexcept { return outline_.has_value(); }

    // True when the fonts have an icon of that name.
    [[nodiscard]] bool has(std::string_view name);

    // Draws the icon centered in the size x size box at (x, y), logical pixels. An unknown name
    // draws the "file" icon and is logged once, so a typo shows up as a wrong icon rather than a
    // hole.
    void draw(Renderer& renderer, std::string_view name, float x, float y, float size, Color color);

private:
    struct Glyph {
        std::uint32_t font{0};
        std::uint32_t index{0};  // 0: none
    };

    Glyph glyph(std::string_view name);

    FontSystem& fonts_;
    std::optional<std::uint32_t> outline_;
    std::optional<std::uint32_t> filled_;
    std::unordered_map<std::string, Glyph> glyphs_;
};

}  // namespace nordend::ui
