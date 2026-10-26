#include "nordend/ui/icons.hpp"

#include <cstdlib>
#include <string>
#include <system_error>
#include <vector>

#include "nordend/core/log.hpp"

namespace nordend::ui {
namespace {

std::vector<std::filesystem::path> data_dirs() {
    std::vector<std::filesystem::path> dirs;
    // NOLINTBEGIN(concurrency-mt-unsafe): read on the UI thread at startup
    const char* data_home = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    const char* data_dirs_env = std::getenv("XDG_DATA_DIRS");
    // NOLINTEND(concurrency-mt-unsafe)
    if (data_home != nullptr && data_home[0] == '/') {
        dirs.emplace_back(data_home);
    } else if (home != nullptr && home[0] != '\0') {
        dirs.emplace_back(std::filesystem::path{home} / ".local/share");
    }
    std::string_view list = (data_dirs_env != nullptr && data_dirs_env[0] != '\0')
                                ? data_dirs_env
                                : "/usr/local/share:/usr/share";
    while (!list.empty()) {
        const std::size_t colon = list.find(':');
        const std::string_view dir = list.substr(0, colon);
        if (!dir.empty() && dir.front() == '/') {
            dirs.emplace_back(dir);
        }
        list = colon == std::string_view::npos ? std::string_view{} : list.substr(colon + 1);
    }
    return dirs;
}

}  // namespace

std::vector<std::filesystem::path> IconSet::search_paths() {
    const std::vector<std::filesystem::path> dirs = data_dirs();
    std::vector<std::filesystem::path> paths;
    paths.reserve(dirs.size() * 2);
    for (const auto& dir : dirs) {
        paths.push_back(dir / "nordend/fonts/tabler-icons.ttf");
    }
    for (const auto& dir : dirs) {
        paths.push_back(dir / "noctalia/assets/fonts/noctalia-tabler.ttf");
    }
    return paths;
}

IconSet::IconSet(FontSystem& fonts) : fonts_{fonts} {
    for (const std::filesystem::path& candidate : search_paths()) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(candidate, error)) {
            continue;
        }
        font_ = fonts_.add_font_file(candidate.string());
        if (font_) {
            path_ = candidate;
            log::info("icons: {}", path_.string());
            return;
        }
    }
    log::warn("icons: no Tabler icon font found (install Noctalia, or see IconSet); no icons");
}

std::uint32_t IconSet::glyph(std::string_view name) {
    if (!font_) {
        return 0;
    }
    std::string key{name};
    if (const auto it = glyphs_.find(key); it != glyphs_.end()) {
        return it->second;
    }
    const std::uint32_t index = fonts_.glyph_by_name(*font_, name);
    glyphs_.emplace(std::move(key), index);
    return index;
}

bool IconSet::has(std::string_view name) { return glyph(name) != 0; }

void IconSet::draw(Renderer& renderer, std::string_view name, float x, float y, float size,
                   Color color) {
    const std::optional<std::uint32_t> font = font_;
    if (!font) {
        return;
    }
    std::uint32_t index = glyph(name);
    if (index == 0) {
        index = glyph("file");
    }
    const TextLayout layout = fonts_.glyph_layout(*font, index, size, renderer.scale());
    renderer.draw_layout(layout, x, y, color);
}

}  // namespace nordend::ui
