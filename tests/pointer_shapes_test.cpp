#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <doctest/doctest.h>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/fs/places.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/offscreen.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/views/browser.hpp"
#include "temp_dir.hpp"

using nordend::core::EventLoop;
using nordend::fs::Place;
using nordend::test::TempDir;
using nordend::ui::CursorShape;
using nordend::ui::KeyEvent;
using nordend::ui::Modifiers;
using nordend::ui::PointerEvent;
using nordend::views::Browser;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

constexpr std::int32_t kWidth = 1000;
constexpr std::int32_t kHeight = 700;

bool run_until(EventLoop& loop, const std::function<bool()>& done) {
    bool ok = false;
    const auto poll = loop.add_timer(1ms, 1ms, [&] {
        if (done()) {
            ok = true;
            loop.quit(0);
        }
    });
    const auto watchdog = loop.add_timer(5s, 0ms, [&loop] { loop.quit(1); });
    loop.run();
    loop.remove(poll);
    loop.remove(watchdog);
    return ok;
}

KeyEvent key(xkb_keysym_t keysym, Modifiers modifiers = {}) {
    KeyEvent event;
    event.keysym = keysym;
    event.modifiers = modifiers;
    return event;
}

std::unique_ptr<nordend::ui::Offscreen> offscreen() {
    try {
        return std::make_unique<nordend::ui::Offscreen>(kWidth, kHeight);
    } catch (const std::runtime_error& error) {
        MESSAGE("no offscreen OpenGL: " << error.what());
        return nullptr;
    }
}

struct Point {
    float x;
    float y;
};

// root/{photos/{a.jpg, b.txt}} with a mounted external disk in the sidebar, drawn offscreen so
// the hit tests find what is shown.
struct Fixture {
    TempDir dir;
    fs::path root = dir.path / "root";
    fs::path photos = root / "photos";
    std::unique_ptr<nordend::ui::Offscreen> gl = offscreen();
    std::unique_ptr<nordend::ui::FontSystem> fonts;
    std::unique_ptr<nordend::ui::Renderer> renderer;
    std::unique_ptr<nordend::ui::IconSet> icons;
    EventLoop loop;
    nordend::core::WorkerPool pool{loop, 2};
    nordend::fs::DirectoryWatcher watcher{loop};
    std::vector<CursorShape> sent;  // what the seat was asked for, in order
    Browser browser{loop,    pool,
                    watcher, nordend::fs::MimeDatabase::load(),
                    [] {},   [this](CursorShape shape) { sent.push_back(shape); }};

    Fixture() {
        fs::create_directories(photos);
        std::ofstream{photos / "a.jpg"} << "a";
        std::ofstream{photos / "b.txt"} << "b";
        browser.set_home_trash(dir.path / "Trash");
        browser.set_places({{.section = Place::Section::places,
                             .label = "Root",
                             .path = root,
                             .icon = "folder",
                             .disk = {}},
                            {.section = Place::Section::devices,
                             .label = "kinetic",
                             .path = photos,
                             .icon = "usb",
                             .disk = {.volume = "/dev/sdb1", .mounted = true, .ejectable = true}}});
        if (gl) {
            fonts = std::make_unique<nordend::ui::FontSystem>();
            renderer = std::make_unique<nordend::ui::Renderer>(*fonts);
            icons = std::make_unique<nordend::ui::IconSet>(*fonts);
        }
    }

    void draw() {
        renderer->begin_frame(kWidth, kHeight, 1.0F, {});
        browser.draw(*renderer, {0.0F, 0.0F, kWidth, kHeight}, nordend::theme::Palette{}, *icons,
                     Browser::Clock::now());
        renderer->end_frame();
    }

    bool settle() {
        const bool done = run_until(loop, [this] { return !browser.loading(); });
        draw();
        return done;
    }

    void press(const KeyEvent& event) {
        CHECK(browser.key(event, Browser::Clock::now()));
        REQUIRE(settle());
    }

    // The first point, scanning the window row by row, where the shape is shape and where
    // pick(x, y) says yes; nullopt when there is none.
    [[nodiscard]] std::optional<Point> find(
        CursorShape shape, const std::function<bool(float, float)>& pick = {}) const {
        for (float y = 1.0F; y < kHeight; y += 2.0F) {
            for (float x = 1.0F; x < kWidth; x += 2.0F) {
                if (browser.cursor_at(x, y) == shape && (!pick || pick(x, y))) {
                    return Point{x, y};
                }
            }
        }
        return std::nullopt;
    }

    void move(Point point) {
        browser.pointer({.kind = PointerEvent::Kind::motion, .x = point.x, .y = point.y},
                        Browser::Clock::now());
    }
};

}  // namespace

TEST_CASE("pointer shapes: a hand on every kind of button, an arrow elsewhere") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.browser.open(f.photos);
    REQUIRE(f.settle());
    f.press(key(XKB_KEY_Down));  // a.jpg: the info pane shows it, with "More details"

    const nordend::views::StatusBar& status = f.browser.status_bar();
    const nordend::views::Breadcrumb& path = f.browser.breadcrumb();
    const nordend::views::Sidebar& sidebar = f.browser.sidebar();

    // The status bar's buttons are hands; its text is not.
    CHECK(f.find(CursorShape::pointer, [&](float x, float y) { return status.claims(x, y); }));
    CHECK(f.find(CursorShape::standard, [&](float x, float y) { return status.claims(x, y); }));
    // The crumbs that open a folder are hands; the folder shown and the bar around are not.
    CHECK(f.find(CursorShape::pointer, [&](float x, float y) { return path.claims(x, y); }));
    CHECK(f.find(CursorShape::standard, [&](float x, float y) { return path.claims(x, y); }));
    // The disk's eject icon is a hand; the places themselves are arrows.
    CHECK(f.find(CursorShape::pointer, [&](float x, float y) { return sidebar.claims(x, y); }));
    CHECK(f.find(CursorShape::standard, [&](float x, float y) { return sidebar.claims(x, y); }));
    // "More details", in the view, once the file's details are read (on a worker).
    const auto in_view = [&](float x, float y) {
        return !status.claims(x, y) && !path.claims(x, y) && !sidebar.claims(x, y);
    };
    CHECK(run_until(f.loop, [&] {
        f.draw();
        return f.find(CursorShape::pointer, in_view).has_value();
    }));
    CHECK_FALSE(f.find(CursorShape::text));  // no field open
}

TEST_CASE("pointer shapes: the text cursor over a name being edited") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.browser.open(f.photos);
    REQUIRE(f.settle());
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_F2));
    REQUIRE(f.browser.name_editor().active());
    const nordend::ui::Rect field = f.browser.name_editor().field().bounds();
    REQUIRE_FALSE(field.empty());
    CHECK(f.browser.cursor_at(field.x + (field.width / 2.0F), field.y + (field.height / 2.0F)) ==
          CursorShape::text);
    CHECK(f.browser.cursor_at(field.x + (field.width / 2.0F), field.bottom() + 40.0F) !=
          CursorShape::text);
}

TEST_CASE("pointer shapes: a card takes them; under a menu, an arrow") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.browser.open(f.photos);
    REQUIRE(f.settle());
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_Delete, {.shift = true}));  // asks before deleting for good
    const nordend::views::StatusBar& status = f.browser.status_bar();
    // Its buttons are hands; the status bar under it is no longer clickable.
    CHECK(f.find(CursorShape::pointer));
    CHECK_FALSE(
        f.find(CursorShape::pointer, [&](float x, float y) { return status.claims(x, y); }));
    f.press(key(XKB_KEY_Escape));

    f.press(key(XKB_KEY_Menu));  // the context menu: its rows are arrows, as menus are
    CHECK_FALSE(f.find(CursorShape::pointer));
}

TEST_CASE("pointer shapes: sent when they change, the resize arrow first") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.browser.open(f.photos);
    REQUIRE(f.settle());
    f.press(key(XKB_KEY_Down));
    const nordend::views::StatusBar& status = f.browser.status_bar();
    const std::optional<Point> button =
        f.find(CursorShape::pointer, [&](float x, float y) { return status.claims(x, y); });
    REQUIRE(button);

    f.move({500.0F, 300.0F});
    f.move({502.0F, 301.0F});  // same shape: nothing sent again
    REQUIRE_FALSE(f.sent.empty());
    CHECK(f.sent.back() == CursorShape::standard);
    const std::size_t before = f.sent.size();
    f.move(button.value_or(Point{0.0F, 0.0F}));
    CHECK(f.sent.size() == before + 1);
    CHECK(f.sent.back() == CursorShape::pointer);
    f.move({500.0F, 300.0F});
    CHECK(f.sent.back() == CursorShape::standard);

    // The sidebar's edge: its resize arrow, not the shape under it.
    const float edge = f.browser.sidebar().width();
    f.move({edge, 300.0F});
    CHECK(f.sent.back() == CursorShape::ew_resize);

    // Leaving: the next shape is sent again, whoever set one meanwhile.
    f.browser.pointer({.kind = PointerEvent::Kind::leave}, Browser::Clock::now());
    const std::size_t left = f.sent.size();
    f.move({500.0F, 300.0F});
    CHECK(f.sent.size() == left + 1);
    CHECK(f.sent.back() == CursorShape::standard);
}

TEST_CASE("pointer shapes: the gear opens the settings; a hand on their values, text in one") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.browser.open(f.photos);
    REQUIRE(f.settle());
    const nordend::views::StatusBar& status = f.browser.status_bar();
    // The rightmost hand of the status bar is the gear.
    std::optional<Point> gear;
    for (float x = kWidth - 1.0F; x > 0.0F && !gear; x -= 1.0F) {
        const float y = kHeight - (nordend::views::StatusBar::kHeight / 2.0F);
        if (status.claims(x, y) && f.browser.cursor_at(x, y) == CursorShape::pointer) {
            gear = Point{x, y};
        }
    }
    REQUIRE(gear);
    const Point at = gear.value_or(Point{0.0F, 0.0F});
    f.browser.pointer({.kind = PointerEvent::Kind::press,
                       .x = at.x,
                       .y = at.y,
                       .button = nordend::ui::PointerButton::left,
                       .click_count = 1},
                      Browser::Clock::now());
    REQUIRE(f.browser.settings_panel().is_open());
    f.draw();
    // Its values are hands (at the right; the page titles at the left are too), the rest of the
    // card and the window under it arrows.
    const std::optional<Point> value =
        f.find(CursorShape::pointer, [](float x, float /*y*/) { return x > kWidth / 2.0F; });
    REQUIRE(value);
    CHECK(f.find(CursorShape::standard));
    CHECK_FALSE(
        f.find(CursorShape::pointer, [&](float x, float y) { return status.claims(x, y); }));
    // Clicking one changes it: the text cursor over it.
    const Point box = value.value_or(Point{0.0F, 0.0F});
    f.browser.pointer({.kind = PointerEvent::Kind::press,
                       .x = box.x + 20.0F,
                       .y = box.y + 8.0F,
                       .button = nordend::ui::PointerButton::left,
                       .click_count = 1},
                      Browser::Clock::now());
    CHECK(f.browser.settings_panel().editing());
    f.draw();
    CHECK(f.browser.cursor_at(box.x + 20.0F, box.y + 8.0F) == CursorShape::text);
}
