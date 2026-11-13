#include "rummage/ui/scrollbar.hpp"

#include <algorithm>

namespace rummage::ui {

float Scrollbar::length(const Rect& rect) const {
    return axis_ == Axis::vertical ? rect.height : rect.width;
}

float Scrollbar::start(const Rect& rect) const { return axis_ == Axis::vertical ? rect.y : rect.x; }

void Scrollbar::layout(const Rect& viewport) {
    constexpr float inset = 5.0F;
    constexpr float width = 12.0F;
    viewport_length_ = length(viewport);
    track_ = axis_ == Axis::vertical
                 ? Rect{viewport.right() - inset - width, viewport.y, width, viewport.height}
                 : Rect{viewport.x, viewport.bottom() - inset - width, viewport.width, width};
    track_ = track_.intersect(viewport);
}

Rect Scrollbar::thumb(const Scroller& scroller) const {
    if (track_.empty() || scroller.max_offset() <= 0.0F) {
        return {};
    }
    const float size =
        std::min(length(track_), std::max(24.0F, length(track_) * viewport_length_ /
                                                     (viewport_length_ + scroller.max_offset())));
    const float position =
        start(track_) + (length(track_) - size) * scroller.offset() / scroller.max_offset();
    // The thumb is six pixels wide; the full twelve-pixel strip accepts the pointer.
    if (axis_ == Axis::vertical) {
        return {track_.x + track_.width / 4.0F, position, track_.width / 2.0F, size};
    }
    return {position, track_.y + track_.height / 4.0F, size, track_.height / 2.0F};
}

bool Scrollbar::hit(const Scroller& scroller, float x, float y) const {
    return !thumb(scroller).empty() && track_.contains(x, y);
}

void Scrollbar::move(float position, Scroller& scroller) {
    const float size = length(thumb(scroller));
    const float travel = length(track_) - size;
    if (travel > 0.0F) {
        scroller.jump_to((position - start(track_) - grab_fraction_ * size) *
                         scroller.max_offset() / travel);
    }
}

bool Scrollbar::pointer(const PointerEvent& event, Scroller& scroller) {
    using Kind = PointerEvent::Kind;
    const float position = axis_ == Axis::vertical ? event.y : event.x;
    if (event.kind == Kind::leave) {
        dragging_ = false;
        return false;
    }
    if (dragging_) {
        if (event.kind == Kind::motion) {
            move(position, scroller);
        } else if (event.kind == Kind::release && event.button == PointerButton::left) {
            move(position, scroller);
            dragging_ = false;
        }
        return true;
    }
    if (event.kind != Kind::press || event.button != PointerButton::left ||
        !hit(scroller, event.x, event.y)) {
        return false;
    }
    const Rect handle = thumb(scroller);
    const float relative = position - start(handle);
    const bool on_thumb = relative >= 0.0F && relative < length(handle);
    grab_fraction_ = on_thumb ? relative / length(handle) : 0.5F;
    dragging_ = true;
    scroller.jump_to(scroller.offset());  // stop wheel or touchpad inertia immediately
    if (!on_thumb) {
        move(position, scroller);  // click the track to jump there, then continue dragging
    }
    return true;
}

}  // namespace rummage::ui
