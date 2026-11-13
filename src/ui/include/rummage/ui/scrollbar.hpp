#pragma once

#include "rummage/ui/geometry.hpp"
#include "rummage/ui/scroller.hpp"
#include "rummage/ui/seat.hpp"

namespace rummage::ui {

// A scrollbar with a generous hit strip, inset clear of pane resize handles. The view draws
// thumb() and sends pointer events before handling files; true means the gesture was consumed.
class Scrollbar {
public:
    enum class Axis { vertical, horizontal };

    explicit Scrollbar(Axis axis = Axis::vertical) : axis_{axis} {}

    void layout(const Rect& viewport);
    [[nodiscard]] Rect thumb(const Scroller& scroller) const;
    [[nodiscard]] bool hit(const Scroller& scroller, float x, float y) const;

    [[nodiscard]] bool dragging() const noexcept { return dragging_; }

    bool pointer(const PointerEvent& event, Scroller& scroller);

private:
    [[nodiscard]] float length(const Rect& rect) const;
    [[nodiscard]] float start(const Rect& rect) const;
    void move(float position, Scroller& scroller);

    Axis axis_;
    Rect track_{};
    float viewport_length_{0.0F};
    float grab_fraction_{0.0F};
    bool dragging_{false};
};

}  // namespace rummage::ui
