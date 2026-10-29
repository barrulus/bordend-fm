#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/renderer.hpp"

namespace nordend::views {

// The groups the overlay shows, column by column: a shortcut in another group would not be shown.
[[nodiscard]] std::vector<std::string_view> overlay_groups();

// Every keyboard shortcut (shortcuts()), by group, on a card over the window. F1, "?", the
// status bar's "?" open it; those, Escape or a click close it.
//
// Loop thread only.
class ShortcutsOverlay {
public:
    explicit ShortcutsOverlay(std::function<void()> redraw);

    void open();
    void close();

    [[nodiscard]] bool is_open() const noexcept { return open_; }

    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette) const;

private:
    std::function<void()> redraw_;
    bool open_{false};
};

}  // namespace nordend::views
