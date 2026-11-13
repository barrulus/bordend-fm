#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/ui/window.hpp"

namespace nordend::views {

// What the title bar asks of the window; wired by main to ui::Window, faked in tests.
struct WindowControls {
    std::function<void()> move;
    std::function<void(ui::Edge edge)> resize;
    std::function<void()> minimize;
    std::function<void()> toggle_maximized;
    std::function<void(float x, float y)> window_menu;
    std::function<void()> close;
};

// The title bar and the resize edges Nordend draws itself when the compositor draws none
// (client-side decorations, ROADMAP 7.4): the title in the middle, minimize, maximize and close
// at the right. Dragging the bar moves the window, a double click maximizes it, a right click
// opens the compositor's window menu, and a band along the window's edges resizes it, with the
// matching cursor. Maximized or fullscreen the edges do not resize; fullscreen there is no bar.
//
// Integrated (ROADMAP 3⅞, the default): no bar of its own. The buttons sit at the right of the
// Browser's header row, and the window moves from where the Browser says (its header's free
// space, the top of its sidebar); the content gets the whole window.
//
// Hit tests use the last layout() (draw() lays out). Loop thread only.
class TitleBar {
public:
    static constexpr float kHeight = 36.0F;
    static constexpr float kIntegratedHeight = 40.0F;  // the content's header row
    static constexpr float kEdge = 5.0F;               // the resize band, inside the window
    static constexpr float kCorner = 16.0F;            // how far a corner reaches along its edges

    enum class Part : std::uint8_t { none, title, minimize, maximize, close, edge };

    struct Hit {
        Part part{Part::none};
        ui::Edge edge{ui::Edge::top};  // when part is edge

        bool operator==(const Hit&) const = default;
    };

    TitleBar(WindowControls controls, std::function<void()> redraw,
             std::function<void(ui::CursorShape)> set_cursor);

    void set_state(const ui::WindowState& state);

    // Integrated into the content's header: drag_area tells where a press moves the window.
    void set_integrated(std::function<bool(float x, float y)> drag_area);

    [[nodiscard]] bool integrated() const noexcept { return static_cast<bool>(drag_area_); }

    // The width the buttons take at the window's right, with their margins: the header keeps it
    // free. 0 before a layout or in fullscreen.
    [[nodiscard]] float buttons_width() const;

    // Where the bar and the buttons go in bounds (the whole window).
    void layout(const ui::Rect& bounds);

    // What is left for the content under the bar.
    [[nodiscard]] ui::Rect content(const ui::Rect& bounds) const;

    [[nodiscard]] Hit hit(float x, float y) const;

    // A pointer event over the window. True when the bar or an edge took it: the content does
    // not get it then.
    bool pointer(const ui::PointerEvent& event);

    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, std::string_view title);

private:
    [[nodiscard]] ui::Rect button(Part part) const;
    void draw_buttons(ui::Renderer& renderer, const theme::Palette& palette, ui::IconSet& icons,
                      ui::Color text) const;
    void hover(const Hit& hit);

    std::function<bool(float x, float y)> drag_area_;
    WindowControls controls_;
    std::function<void()> redraw_;
    std::function<void(ui::CursorShape)> set_cursor_;
    ui::WindowState state_;
    ui::Rect bounds_{};
    ui::Rect bar_{};
    ui::Rect minimize_{};  // empty when the compositor cannot minimize
    ui::Rect maximize_{};
    ui::Rect close_{};
    Hit hovered_;
    Part pressed_{Part::none};  // a button held down
    bool edge_cursor_{false};   // the cursor shows an edge's arrows
};

}  // namespace nordend::views
