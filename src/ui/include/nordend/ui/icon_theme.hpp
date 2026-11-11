#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "nordend/picture/image.hpp"

namespace nordend::ui {

// Desktop settings and icon search roots, read at startup. Explicit roots make lookup testable.
[[nodiscard]] std::string desktop_icon_theme();
[[nodiscard]] std::vector<std::filesystem::path> icon_theme_roots();

// Freedesktop icon themes: size selection, split roots, inheritance and hicolor fallback.
// No GL resources. SVG/PNG rendering is for installed theme assets, not previewing user files.
class IconTheme {
public:
    IconTheme(std::string name, std::vector<std::filesystem::path> roots);
    [[nodiscard]] std::filesystem::path find(std::string_view icon, int pixels) const;
    // Optional diagnostic for callers that deduplicate SVG filter warnings by filename.
    [[nodiscard]] static picture::Image render(const std::filesystem::path& file, int pixels,
                                               bool* unsupported_svg_filter = nullptr);

private:
    struct Directory {
        std::string name;
        int min{0};
        int max{0};
    };

    struct Theme {
        std::string name;
        std::vector<Directory> directories;
    };

    void add_theme(const std::string& name);
    std::vector<std::filesystem::path> roots_;
    std::vector<Theme> themes_;
};

}  // namespace nordend::ui
