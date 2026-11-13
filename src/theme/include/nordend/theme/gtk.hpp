#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nordend/theme/palette.hpp"

// The user's GTK colours: the named colours (@define-color) of their gtk.css, as GTK apps such
// as Thunar follow them. Whoever themes GTK writes them there: Noctalia's GTK template, Stylix,
// matugen, a hand-written file. Nordend reads them as GTK would and knows no desktop by name.
namespace nordend::theme {

// One statement of a stylesheet that matters here, in file order.
struct GtkStatement {
    enum class Kind : std::uint8_t { define, import };
    Kind kind{Kind::define};
    std::string name;   // define: the colour's name
    std::string value;  // define: the value as written; import: the file, as written
};

// The @define-color and @import statements of a stylesheet's text; everything else (rules,
// other at-rules, comments) is skipped.
[[nodiscard]] std::vector<GtkStatement> parse_gtk_css(std::string_view text);

// One colour value: #rgb, #rgba, #rrggbb, #rrggbbaa, rgb(), rgba() (numbers or percentages),
// transparent, white, black, or @name looked up in colors. nullopt for anything else
// (shade(), alpha(), mix(), other names): GTK computes those, Nordend leaves them out.
[[nodiscard]] std::optional<Color> parse_gtk_color(
    std::string_view value, const std::map<std::string, Color, std::less<>>& colors);

struct GtkColors {
    std::map<std::string, Color, std::less<>> colors;
    std::filesystem::path source;              // the gtk.css they came from; empty: none
    std::vector<std::filesystem::path> files;  // every stylesheet read: the source and imports

    bool operator==(const GtkColors&) const = default;
};

// $XDG_CONFIG_HOME/gtk-4.0/gtk.css, then gtk-3.0/gtk.css (else under $HOME/.config).
[[nodiscard]] std::vector<std::filesystem::path> default_gtk_css_paths();

// The first of these stylesheets that defines a colour, with what it imports (in order, a later
// definition replacing an earlier one, as in CSS). Missing or unreadable files are skipped.
[[nodiscard]] GtkColors load_gtk_colors(const std::vector<std::filesystem::path>& candidates);

// The palette made from them: their window, view, sidebar, card, popover, text, accent and
// error colours where they are given, the other roles derived around them (OKLCH) as for the
// desktop's accent. Dark or light from the background itself; dark_hint only when the colours
// give an accent but no background. nullopt when they give neither a background with its text
// nor an accent.
[[nodiscard]] std::optional<Palette> gtk_palette(const GtkColors& gtk, bool dark_hint);

// The accent the colours give, opaque; nullopt without one.
[[nodiscard]] std::optional<Color> gtk_accent(const GtkColors& gtk);

}  // namespace nordend::theme
