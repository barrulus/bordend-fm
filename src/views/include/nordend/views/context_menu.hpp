#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/seat.hpp"

namespace nordend::views {

// One line of a context menu.
struct MenuItem {       // NOLINT(misc-no-recursion): a submenu's items are MenuItems too
    std::string id;     // what tests and set_submenu() find it by ("open_with")
    std::string label;  // "Open With"
    std::string keys;   // the shortcut shown at the right ("Ctrl+C"); may be empty
    bool enabled{true};
    bool separator{false};   // a line above it
    bool submenu{false};     // opens children instead of running action
    std::string empty_text;  // a submenu without children shows it, dimmed ("Loading…")
    std::vector<MenuItem> children;
    std::function<void()> action;  // runs once the menu has closed
};

// A menu over the window at a point (a right click, the Menu key), drawn by Nordend with the
// theme's colors: items, separators, the shortcut of each, one level of submenus. Moved to stay
// whole near the window's edges.
//
// While open it takes every key and every click: Up and Down move (over what cannot be chosen),
// Home and End, Enter runs an item or opens its submenu, Right opens a submenu, Left and Escape
// go back out of it, Escape closes. The pointer highlights what it is over (a submenu opens
// when hovered), a click runs an item, a click outside closes the menu (and does nothing else).
//
// Loop thread only.
class ContextMenu {
public:
    explicit ContextMenu(std::function<void()> redraw);

    // At (x, y), top-left; from the keyboard, the first item is highlighted at once.
    void open(float x, float y, std::vector<MenuItem> items, bool keyboard);

    void close();

    [[nodiscard]] bool is_open() const noexcept { return !items_.empty(); }

    [[nodiscard]] const std::vector<MenuItem>& items() const noexcept { return items_; }

    // Fills the submenu of the item called id (applications, templates found on a worker).
    void set_submenu(std::string_view id, std::vector<MenuItem> children);

    // The highlighted item of the menu and of the open submenu (tests).
    [[nodiscard]] std::optional<std::size_t> highlighted() const noexcept { return main_; }

    [[nodiscard]] std::optional<std::size_t> sub_highlighted() const noexcept { return sub_; }

    [[nodiscard]] bool submenu_open() const noexcept { return open_sub_.has_value(); }

    // Every key while open: true.
    bool key(const ui::KeyEvent& event);

    // Every event while open: true.
    bool pointer(const ui::PointerEvent& event);

    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette);

private:
    [[nodiscard]] static bool choosable(const MenuItem& item) {
        return item.enabled && (!item.submenu || !item.children.empty());
    }

    // The next choosable item from `from` in direction step (+1, -1), wrapping.
    [[nodiscard]] static std::optional<std::size_t> step(const std::vector<MenuItem>& items,
                                                         std::optional<std::size_t> from,
                                                         int direction);
    [[nodiscard]] const std::vector<MenuItem>* sub_items() const;
    void open_submenu(std::size_t index, bool keyboard);
    void choose(const MenuItem& item);
    // The panels' sizes for items, in logical pixels.
    [[nodiscard]] static ui::Rect measure(ui::Renderer& renderer,
                                          const std::vector<MenuItem>& items);
    static void draw_panel(ui::Renderer& renderer, const ui::Rect& panel,
                           const std::vector<MenuItem>& items,
                           std::optional<std::size_t> highlighted, std::vector<ui::Rect>& rects,
                           const theme::Palette& palette);

    std::function<void()> redraw_;
    std::vector<MenuItem> items_;
    float x_{0.0F};
    float y_{0.0F};
    std::optional<std::size_t> main_;      // highlighted in the menu
    std::optional<std::size_t> open_sub_;  // the item whose submenu is open
    std::optional<std::size_t> sub_;       // highlighted in the submenu
    bool in_sub_{false};                   // the keyboard is in the submenu

    // As last drawn, for the pointer.
    ui::Rect panel_{};
    ui::Rect sub_panel_{};
    std::vector<ui::Rect> rects_;
    std::vector<ui::Rect> sub_rects_;
};

}  // namespace nordend::views
