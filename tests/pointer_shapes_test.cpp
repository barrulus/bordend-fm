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
        return !status.claims(x, y) && !path.claims(x, y) && !sidebar.claims(x, y) &&
               !f.browser.new_folder_bounds().contains(x, y);
    };
    CHECK(run_until(f.loop, [&] {
        f.draw();
        return f.find(CursorShape::pointer, in_view).has_value();
    }));
    CHECK_FALSE(f.find(CursorShape::text, [&](float x, float y) {
        return !f.browser.filter_bounds().contains(x, y);
    }));  // only the always-visible name filter
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

TEST_CASE("pointer shapes: sidebar zoom scales eject targets and routes wheel independently") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.browser.open(f.photos);
    REQUIRE(f.settle());
    auto& sidebar = f.browser.sidebar();
    const auto before = sidebar.eject_icon(f.photos);
    REQUIRE(before);
    const auto view_size = f.browser.details().list_size();
    nordend::ui::PointerEvent wheel;
    wheel.kind = nordend::ui::PointerEvent::Kind::scroll;
    wheel.x = before->x + before->width / 2.0F;
    wheel.y = before->y + before->height / 2.0F;
    wheel.modifiers.ctrl = true;
    wheel.wheel_y = -1.0F;
    f.browser.pointer(wheel, Browser::Clock::now());
    f.draw();
    CHECK(sidebar.list_size() == 2);
    CHECK(f.browser.details().list_size() == view_size);
    const auto after = sidebar.eject_icon(f.photos);
    REQUIRE(after);
    CHECK(after->width > before->width);
    const float x = after->x + after->width / 2.0F;
    const float y = after->y + after->height / 2.0F;
    REQUIRE(sidebar.place_under(x, y));
    CHECK(sidebar.place_under(x, y)->path == f.photos);
    CHECK(sidebar.clickable_at(x, y));
    const auto saved = f.browser.state();
    sidebar.set_list_size(0);
    f.browser.apply(saved);
    CHECK(sidebar.list_size() == 2);
}

TEST_CASE("toolbar: filter and New Folder buttons work in each view") {
    for (const auto mode : {Browser::Mode::columns, Browser::Mode::grid, Browser::Mode::details}) {
        Fixture f;
        if (!f.gl) {
            return;
        }
        f.browser.set_mode(mode);
        f.browser.open(f.photos);
        REQUIRE(f.settle());
        const auto click = [&](const nordend::ui::Rect& rect) {
            const float x = rect.x + rect.width / 2;
            const float y = rect.y + rect.height / 2;
            f.browser.pointer({.kind = PointerEvent::Kind::press,
                               .x = x,
                               .y = y,
                               .button = nordend::ui::PointerButton::left},
                              Browser::Clock::now());
            f.browser.pointer({.kind = PointerEvent::Kind::release,
                               .x = x,
                               .y = y,
                               .button = nordend::ui::PointerButton::left},
                              Browser::Clock::now());
        };
        auto field = f.browser.filter_bounds();
        CHECK(f.browser.cursor_at(field.x + 4, field.y + 4) == CursorShape::text);
        click(field);
        REQUIRE(f.browser.filtering());
        f.browser.key({.keysym = XKB_KEY_b, .text = "b"}, Browser::Clock::now());
        REQUIRE(f.settle());
        REQUIRE(f.browser.picker_folder()->entries().size() == 1);
        CHECK(f.browser.picker_folder()->entries()[0].name == "b.txt");
        const auto button = f.browser.new_folder_bounds();
        CHECK(f.browser.cursor_at(button.x + 4, button.y + 4) == CursorShape::pointer);
        click(button);
        REQUIRE(f.settle());
        CHECK(fs::is_directory(f.photos / "New Folder"));
        CHECK(f.browser.name_filter().empty());
        CHECK(f.browser.name_editor().active());
    }
}
