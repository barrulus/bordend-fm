#include "nordend/theme/gtk.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <set>
#include <string_view>
#include <system_error>
#include <utility>

#include "nordend/theme/desktop.hpp"
#include "oklch.hpp"

namespace nordend::theme {
namespace {

// Bounds for a stylesheet tree: a gtk.css is a few kilobytes and imports one or two files.
constexpr std::size_t kMaxFileBytes = std::size_t{1} << 20U;
constexpr std::size_t kMaxFiles = 32;
constexpr int kMaxImportDepth = 8;
constexpr int kMaxReferenceDepth = 16;

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

std::string_view trim(std::string_view text) {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

std::string lower(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return out;
}

// The text without its /* comments */, strings left as they are.
std::string strip_comments(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    char quote = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (quote != 0) {
            out += c;
            if (c == '\\' && i + 1 < text.size()) {
                out += text[++i];
            } else if (c == quote) {
                quote = 0;
            }
        } else if (c == '"' || c == '\'') {
            quote = c;
            out += c;
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            const std::size_t end = text.find("*/", i + 2);
            if (end == std::string_view::npos) {
                break;  // an unclosed comment runs to the end, as in CSS
            }
            out += ' ';
            i = end + 1;
        } else {
            out += c;
        }
    }
    return out;
}

bool valid_name(std::string_view name) {
    return !name.empty() && std::ranges::all_of(name, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    });
}

std::string_view unquote(std::string_view text) {
    text = trim(text);
    if (text.size() >= 2 && (text.front() == '"' || text.front() == '\'') &&
        text.back() == text.front()) {
        text = text.substr(1, text.size() - 2);
    }
    return text;
}

// The file an @import names: url("x"), url(x) or "x"; media queries after it are ignored.
std::string_view import_target(std::string_view rest) {
    rest = trim(rest);
    if (lower(rest.substr(0, 4)) == "url(") {
        const std::size_t close = rest.find(')');
        return close == std::string_view::npos ? std::string_view{}
                                               : unquote(rest.substr(4, close - 4));
    }
    if (!rest.empty() && (rest.front() == '"' || rest.front() == '\'')) {
        const std::size_t close = rest.find(rest.front(), 1);
        return close == std::string_view::npos ? std::string_view{} : rest.substr(1, close - 1);
    }
    return {};
}

void add_statement(std::string_view statement, std::vector<GtkStatement>& out) {
    statement = trim(statement);
    const auto starts = [&](std::string_view keyword) {
        return statement.size() > keyword.size() &&
               lower(statement.substr(0, keyword.size())) == keyword &&
               is_space(statement[keyword.size()]);
    };
    if (starts("@define-color")) {
        const std::string_view rest = trim(statement.substr(13));
        const auto space =
            static_cast<std::size_t>(std::ranges::find_if(rest, is_space) - rest.begin());
        const std::string_view name = rest.substr(0, space);
        const std::string_view value = trim(rest.substr(std::min(space, rest.size())));
        if (valid_name(name) && !value.empty()) {
            out.push_back({.kind = GtkStatement::Kind::define,
                           .name = std::string{name},
                           .value = std::string{value}});
        }
    } else if (statement.starts_with("@import") &&
               (statement.size() == 7 || is_space(statement[7]) || statement[7] == '"' ||
                statement[7] == '\'')) {
        const std::string_view target = import_target(statement.substr(7));
        if (!target.empty()) {
            out.push_back(
                {.kind = GtkStatement::Kind::import, .name = {}, .value = std::string{target}});
        }
    }
}

// A number, or a percentage of full (100 % = full); nullopt when it is neither.
std::optional<double> number(std::string_view text, double full) {
    text = trim(text);
    const bool percent = text.ends_with('%');
    if (percent) {
        text.remove_suffix(1);
    }
    double value = 0.0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return percent ? value / 100.0 * full : value;
}

std::optional<Color> hex_color(std::string_view digits) {
    std::uint32_t value = 0;
    const auto [end, error] =
        std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
    if (error != std::errc{} || end != digits.data() + digits.size()) {
        return std::nullopt;
    }
    const auto nibble = [&](unsigned shift) { return (value >> shift) & 0xFU; };
    switch (digits.size()) {
        case 3:
        case 4: {
            const bool alpha = digits.size() == 4;
            const unsigned base = alpha ? 4U : 0U;
            const std::uint32_t r = nibble(base + 8U) * 17U;
            const std::uint32_t g = nibble(base + 4U) * 17U;
            const std::uint32_t b = nibble(base) * 17U;
            const std::uint32_t a = alpha ? nibble(0U) * 17U : 0xFFU;
            return Color::rgba((r << 24U) | (g << 16U) | (b << 8U) | a);
        }
        case 6:
            return Color::rgb(value);
        case 8:
            return Color::rgba(value);
        default:
            return std::nullopt;
    }
}

std::optional<Color> functional_color(std::string_view value) {
    const std::string head = lower(value.substr(0, value.find('(')));
    if ((head != "rgb" && head != "rgba") || !value.ends_with(')')) {
        return std::nullopt;
    }
    std::string_view inside = value.substr(head.size() + 1, value.size() - head.size() - 2);
    std::vector<std::string_view> parts;
    for (std::size_t comma = inside.find(','); comma != std::string_view::npos;
         comma = inside.find(',')) {
        parts.push_back(inside.substr(0, comma));
        inside.remove_prefix(comma + 1);
    }
    parts.push_back(inside);
    if (parts.size() != 3 && parts.size() != 4) {
        return std::nullopt;
    }
    constexpr double full_scale = 255.0;
    std::array<double, 4> channels{0.0, 0.0, 0.0, 1.0};
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const std::optional<double> v = number(parts[i], i < 3 ? full_scale : 1.0);
        if (!v) {
            return std::nullopt;
        }
        channels.at(i) = std::clamp(*v / (i < 3 ? full_scale : 1.0), 0.0, 1.0);
    }
    return Color{static_cast<float>(channels[0]), static_cast<float>(channels[1]),
                 static_cast<float>(channels[2]), static_cast<float>(channels[3])};
}

template <typename Lookup>
// NOLINTNEXTLINE(misc-no-recursion): a reference's depth, bounded by kMaxReferenceDepth
std::optional<Color> parse_color(std::string_view value, const Lookup& lookup) {
    value = trim(value);
    if (value.empty()) {
        return std::nullopt;
    }
    if (value.front() == '@') {
        return lookup(value.substr(1));
    }
    if (value.front() == '#') {
        return hex_color(value.substr(1));
    }
    const std::string word = lower(value);
    if (word == "transparent") {
        return Color{0.0F, 0.0F, 0.0F, 0.0F};
    }
    if (word == "white") {
        return Color::rgb(0xFFFFFF);
    }
    if (word == "black") {
        return Color::rgb(0x000000);
    }
    return functional_color(value);
}

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream in{path, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    // As long as the file, up to the bound: a stylesheet is a few kilobytes.
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    std::string text;
    text.resize(error ? std::size_t{0}
                      : static_cast<std::size_t>(std::min<std::uintmax_t>(size, kMaxFileBytes)));
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(in.gcount()));
    return text;
}

struct Loader {
    std::map<std::string, std::string, std::less<>> raw;  // the last definition of each name
    std::vector<std::filesystem::path> files;
    std::set<std::filesystem::path> seen;

    // NOLINTNEXTLINE(misc-no-recursion): an import's depth, bounded by kMaxImportDepth
    void load(const std::filesystem::path& path, int depth) {
        if (depth > kMaxImportDepth || files.size() >= kMaxFiles) {
            return;
        }
        std::error_code error;
        const std::filesystem::path key = std::filesystem::weakly_canonical(path, error);
        if (!seen.insert(error ? path : key).second) {
            return;  // imported twice, or a cycle
        }
        const std::optional<std::string> text = read_file(path);
        if (!text) {
            return;
        }
        files.push_back(path);
        for (GtkStatement& statement : parse_gtk_css(*text)) {
            if (statement.kind == GtkStatement::Kind::define) {
                raw[std::move(statement.name)] = std::move(statement.value);
                continue;
            }
            std::string_view target = statement.value;
            if (target.starts_with("resource:")) {
                continue;  // inside GTK itself
            }
            if (target.starts_with("file://")) {
                target.remove_prefix(7);
            }
            std::filesystem::path file{target};
            if (file.is_relative()) {
                file = path.parent_path() / file;
            }
            load(file, depth + 1);
        }
    }

    // Every name whose value is a colour, references followed (GTK looks them up at the end).
    [[nodiscard]] std::map<std::string, Color, std::less<>> resolve() const {
        std::map<std::string, Color, std::less<>> colors;
        for (const auto& [name, value] : raw) {
            if (const std::optional<Color> color = resolve_value(value, 0)) {
                colors.emplace(name, *color);
            }
        }
        return colors;
    }

    // NOLINTNEXTLINE(misc-no-recursion): bounded by kMaxReferenceDepth
    [[nodiscard]] std::optional<Color> resolve_value(std::string_view value, int depth) const {
        // NOLINTNEXTLINE(misc-no-recursion)
        return parse_color(value, [&](std::string_view name) -> std::optional<Color> {
            const auto it = raw.find(name);
            if (it == raw.end() || depth >= kMaxReferenceDepth) {
                return std::nullopt;
            }
            return resolve_value(it->second, depth + 1);
        });
    }
};

// c over base, opaque.
Color over(Color c, Color base) {
    const float a = std::clamp(c.a, 0.0F, 1.0F);
    return {(c.r * a) + (base.r * (1.0F - a)), (c.g * a) + (base.g * (1.0F - a)),
            (c.b * a) + (base.b * (1.0F - a)), 1.0F};
}

// From a towards b by t (0 = a, 1 = b), opaque.
Color mix(Color a, Color b, float t) {
    return {a.r + ((b.r - a.r) * t), a.g + ((b.g - a.g) * t), a.b + ((b.b - a.b) * t), 1.0F};
}

// Light or dark text, whichever reads better on the background.
Color on(Color background, Color light, Color dark) {
    return contrast(background, light) >= contrast(background, dark) ? light : dark;
}

// Text that steps back from the main text toward the background, but still reads on it.
Color variant(Color text, Color background, float wanted, float readable) {
    for (float t = wanted; t > 0.0F; t -= 0.05F) {
        const Color candidate = mix(text, background, t);
        if (contrast(candidate, background) >= readable) {
            return candidate;
        }
    }
    return text;
}

}  // namespace

std::vector<GtkStatement> parse_gtk_css(std::string_view text) {
    const std::string clean = strip_comments(text);
    const std::string_view all{clean};
    std::vector<GtkStatement> out;
    std::size_t start = 0;
    int depth = 0;
    char quote = 0;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const char c = all[i];
        if (quote != 0) {
            if (c == '\\') {
                ++i;
            } else if (c == quote) {
                quote = 0;
            }
            continue;
        }
        switch (c) {
            case '"':
            case '\'':
                quote = c;
                break;
            case '(':
            case '[':
                ++depth;
                break;
            case ')':
            case ']':
                depth = std::max(0, depth - 1);
                break;
            case '{':
                ++depth;
                break;
            case '}':
                // The end of a rule's block: nothing in it is a colour definition.
                if (depth > 0) {
                    --depth;
                    if (depth == 0) {
                        start = i + 1;
                    }
                }
                break;
            case ';':
                if (depth == 0) {
                    add_statement(all.substr(start, i - start), out);
                    start = i + 1;
                }
                break;
            default:
                break;
        }
    }
    return out;
}

std::optional<Color> parse_gtk_color(std::string_view value,
                                     const std::map<std::string, Color, std::less<>>& colors) {
    return parse_color(value, [&](std::string_view name) -> std::optional<Color> {
        const auto it = colors.find(name);
        return it == colors.end() ? std::nullopt : std::optional<Color>{it->second};
    });
}

std::vector<std::filesystem::path> default_gtk_css_paths() {
    std::filesystem::path config;
    // NOLINTBEGIN(concurrency-mt-unsafe): read before any thread looks at the environment
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    // NOLINTEND(concurrency-mt-unsafe)
    if (xdg != nullptr && xdg[0] == '/') {
        config = xdg;
    } else if (home != nullptr && home[0] != '\0') {
        config = std::filesystem::path{home} / ".config";
    } else {
        return {};
    }
    return {config / "gtk-4.0" / "gtk.css", config / "gtk-3.0" / "gtk.css"};
}

GtkColors load_gtk_colors(const std::vector<std::filesystem::path>& candidates) {
    for (const std::filesystem::path& candidate : candidates) {
        Loader loader;
        loader.load(candidate, 0);
        std::map<std::string, Color, std::less<>> colors = loader.resolve();
        if (!colors.empty()) {
            return {
                .colors = std::move(colors), .source = candidate, .files = std::move(loader.files)};
        }
    }
    return {};
}

std::optional<Color> gtk_accent(const GtkColors& gtk) {
    for (const std::string_view name :
         {"accent_bg_color", "theme_selected_bg_color", "accent_color"}) {
        if (const auto it = gtk.colors.find(name); it != gtk.colors.end() && it->second.a > 0.0F) {
            return it->second.with_alpha(1.0F);
        }
    }
    return std::nullopt;
}

std::optional<Palette> gtk_palette(const GtkColors& gtk, bool dark_hint) {
    const auto get = [&](std::initializer_list<std::string_view> names) -> std::optional<Color> {
        for (const std::string_view name : names) {
            if (const auto it = gtk.colors.find(name); it != gtk.colors.end()) {
                return it->second;
            }
        }
        return std::nullopt;
    };
    const std::optional<Color> window = get({"window_bg_color", "theme_bg_color"});
    const std::optional<Color> view = get({"view_bg_color", "theme_base_color"});
    const std::optional<Color> text =
        get({"view_fg_color", "window_fg_color", "theme_text_color", "theme_fg_color"});
    const std::optional<Color> accent = gtk_accent(gtk);
    const std::optional<Color> background = window ? window : view;
    if (!(background && text) && !accent) {
        return std::nullopt;
    }

    // The surfaces, opaque: what is translucent in GTK is drawn over the window.
    const Color backdrop = Color::rgb(dark_hint ? 0x000000 : 0xFFFFFF);
    const std::optional<Color> base =
        background ? std::optional<Color>{over(*background, backdrop)} : std::nullopt;
    const std::optional<Color> surface =
        base ? std::optional<Color>{view ? over(*view, *base) : *base} : std::nullopt;
    const bool dark = surface ? to_lch(*surface).l < 0.6 : dark_hint;

    Palette p = desktop_palette({.dark = dark, .accent = accent});
    if (surface && text) {
        // The surfaces GTK does not name, moved by as much as its own background is from the
        // derived one, in its hue: the levels keep their steps around the real colours.
        const Lch real = to_lch(*surface);
        const double shift = real.l - to_lch(p.surface).l;
        const auto moved = [&](Color derived) {
            const Lch lch = to_lch(derived);
            return from_lch({std::clamp(lch.l + shift, 0.0, 1.0), real.c, real.h});
        };
        p.surface_dim = moved(p.surface_dim);
        p.surface_bright = moved(p.surface_bright);
        p.surface_container_lowest = moved(p.surface_container_lowest);
        p.surface_container_low = moved(p.surface_container_low);
        p.surface_container = moved(p.surface_container);
        p.surface_container_high = moved(p.surface_container_high);
        p.surface_container_highest = moved(p.surface_container_highest);
        p.surface = *surface;
        if (const std::optional<Color> sidebar = get({"sidebar_bg_color", "headerbar_bg_color"})) {
            p.surface_container_low = over(*sidebar, *surface);
        }
        if (const std::optional<Color> card = get({"card_bg_color"})) {
            p.surface_container = over(*card, *surface);
        }
        if (const std::optional<Color> popover = get({"popover_bg_color", "dialog_bg_color"})) {
            p.surface_container_high = over(*popover, *surface);
        }
        p.surface_variant = p.surface_container;

        const Color ink = over(*text, *surface);
        p.on_surface = ink;
        p.on_surface_variant = variant(ink, *surface, 0.30F, 4.5F);
        p.outline = variant(ink, *surface, 0.55F, 3.0F);
        p.outline_variant = mix(ink, *surface, 0.82F);
    }
    if (accent) {
        p.primary = *accent;
        const std::optional<Color> accent_text =
            get({"accent_fg_color", "theme_selected_fg_color"});
        p.on_primary = accent_text ? over(*accent_text, *accent)
                                   : on(*accent, Color::rgb(0xFFFFFF), Color::rgb(0x000000));
        p.hover = p.primary;
        p.on_hover = p.on_primary;
    }
    if (const std::optional<Color> error =
            get({"error_color", "destructive_color", "error_bg_color", "destructive_bg_color"})) {
        p.error = over(*error, p.surface);
        const std::optional<Color> error_text = get({"error_fg_color", "destructive_fg_color"});
        p.on_error = error_text ? over(*error_text, p.error)
                                : on(p.error, Color::rgb(0xFFFFFF), Color::rgb(0x000000));
    }
    p.dark = dark;
    return p;
}

}  // namespace nordend::theme
