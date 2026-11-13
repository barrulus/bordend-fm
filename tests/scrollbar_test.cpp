#include "rummage/ui/scrollbar.hpp"

#include <cmath>

#include <doctest/doctest.h>

using rummage::ui::PointerButton;
using rummage::ui::PointerEvent;
using rummage::ui::Scrollbar;
using rummage::ui::Scroller;
using Kind = PointerEvent::Kind;

TEST_CASE("scrollbar: dragging preserves the grab point, stops inertia and clamps outside") {
    for (const auto axis : {Scrollbar::Axis::vertical, Scrollbar::Axis::horizontal}) {
        CAPTURE(axis);
        Scroller scroller;
        scroller.set_extent(10000, 500);
        scroller.jump_to(3000);
        scroller.wheel(3, Scroller::Clock::now());
        Scrollbar bar{axis};
        bar.layout({30, 40, 500, 500});
        const auto thumb = bar.thumb(scroller);
        const bool vertical = axis == Scrollbar::Axis::vertical;
        const float grab = 0.75F * (vertical ? thumb.height : thumb.width);
        const float x = thumb.x + (vertical ? 1.0F : grab);
        const float y = thumb.y + (vertical ? grab : 1.0F);
        CHECK(bar.pointer({.kind = Kind::press, .x = x, .y = y, .button = PointerButton::left},
                          scroller));
        CHECK(bar.dragging());
        CHECK_FALSE(scroller.animating());
        CHECK(scroller.offset() == 3000);
        CHECK(bar.pointer({.kind = Kind::motion, .x = x, .y = y}, scroller));
        CHECK(static_cast<double>(scroller.offset()) == doctest::Approx(3000));
        CHECK(bar.pointer({.kind = Kind::motion, .x = 10000, .y = 10000}, scroller));
        CHECK(scroller.offset() == scroller.max_offset());
        CHECK(bar.pointer(
            {.kind = Kind::release, .x = -100, .y = -100, .button = PointerButton::left},
            scroller));
        CHECK(scroller.offset() == 0);
        CHECK_FALSE(bar.dragging());
        CHECK_FALSE(bar.pointer({.kind = Kind::motion, .x = x, .y = y}, scroller));
        CHECK(scroller.offset() == 0);
    }
}

TEST_CASE("scrollbar: track clicks jump, hit strip excludes resize edge, leave cancels") {
    Scroller scroller;
    scroller.set_extent(5000, 500);
    Scrollbar bar;
    bar.layout({0, 0, 400, 500});
    CHECK(bar.hit(scroller, 384, 250));        // larger than the painted thumb
    CHECK_FALSE(bar.hit(scroller, 397, 250));  // pane resize edge
    CHECK_FALSE(bar.pointer(
        {.kind = Kind::press, .x = 390, .y = 250, .button = PointerButton::right}, scroller));
    CHECK(bar.pointer({.kind = Kind::press, .x = 384, .y = 250, .button = PointerButton::left},
                      scroller));
    CHECK(static_cast<double>(scroller.offset()) == doctest::Approx(2250));
    CHECK_FALSE(bar.pointer({.kind = Kind::leave}, scroller));
    CHECK_FALSE(bar.dragging());
    CHECK_FALSE(bar.pointer({.kind = Kind::motion, .x = 390, .y = 500}, scroller));
    CHECK(static_cast<double>(scroller.offset()) == doctest::Approx(2250));
}

TEST_CASE("scrollbar: short viewports and disappearing overflow stay finite") {
    Scroller scroller;
    scroller.set_extent(1000, 10);
    Scrollbar bar;
    bar.layout({0, 0, 100, 10});
    CHECK(bar.thumb(scroller).height == 10);
    CHECK(bar.pointer({.kind = Kind::press, .x = 90, .y = 2, .button = PointerButton::left},
                      scroller));
    CHECK(bar.pointer({.kind = Kind::motion, .x = 90, .y = 500}, scroller));
    CHECK(std::isfinite(scroller.offset()));
    scroller.set_extent(10, 500);
    CHECK(bar.thumb(scroller).empty());
    CHECK_FALSE(bar.hit(scroller, 90, 2));
    CHECK(bar.pointer({.kind = Kind::release, .x = 90, .y = 500, .button = PointerButton::left},
                      scroller));
    CHECK(scroller.offset() == 0);
    bar.layout({});
    CHECK(bar.thumb(scroller).empty());
}
