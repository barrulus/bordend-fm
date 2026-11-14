#include "rummage/views/info_pane.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <doctest/doctest.h>

#include "rummage/core/event_loop.hpp"
#include "rummage/core/worker_pool.hpp"
#include "rummage/fs/directory_watcher.hpp"
#include "rummage/fs/mime.hpp"
#include "rummage/theme/palette.hpp"
#include "rummage/ui/icons.hpp"
#include "rummage/ui/offscreen.hpp"
#include "rummage/ui/renderer.hpp"
#include "rummage/ui/text.hpp"
#include "rummage/views/browser.hpp"
#include "temp_dir.hpp"

using rummage::core::EventLoop;
using rummage::core::WorkerPool;
using rummage::views::Browser;
using rummage::views::InfoPane;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

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

constexpr std::int32_t kWidth = 1200;
constexpr std::int32_t kHeight = 900;

// dir/{long.txt (200 lines), short.txt (5 lines)}, drawn offscreen.
struct Fixture {
    rummage::test::TempDir dir;
    EventLoop loop;
    WorkerPool pool{loop, 2};
    rummage::fs::DirectoryWatcher watcher{loop};
    Browser browser{loop,    pool,
                    watcher, rummage::fs::MimeDatabase::load(),
                    [] {},   [](rummage::ui::CursorShape /*shape*/) {}};
    rummage::ui::FontSystem fonts;
    rummage::ui::Renderer renderer{fonts};
    rummage::ui::IconSet icons{fonts};
    Browser::Clock::time_point now = Browser::Clock::now();
    std::int32_t height{kHeight};

    Fixture() {
        std::ofstream longer{dir.path / "long.txt"};
        for (int i = 0; i < 200; ++i) {
            longer << "line " << i << " of a file that goes on\n";
        }
        longer.close();
        std::ofstream shorter{dir.path / "short.txt"};
        for (int i = 0; i < 5; ++i) {
            shorter << "short " << i << "\n";
        }
        shorter.close();
        browser.open(dir.path);
    }

    InfoPane& pane() { return browser.info_pane(); }

    // A frame, `later` after the last one: scrolls glide to where they were sent.
    void draw(std::chrono::milliseconds later = 0ms) {
        now += later;
        renderer.begin_frame(kWidth, height, 1.0F, {});
        browser.draw(renderer, {0.0F, 0.0F, kWidth, static_cast<float>(height)},
                     rummage::theme::Palette{}, icons, now);
        renderer.end_frame();
    }

    // Drawn until the pane has read what is selected.
    bool settle() {
        draw();
        const bool ok = run_until(loop, [this] { return !browser.loading(); });
        draw();
        return ok;
    }

    void press(xkb_keysym_t key, rummage::ui::Modifiers modifiers = {}) {
        rummage::ui::KeyEvent event;
        event.keysym = key;
        event.modifiers = modifiers;
        browser.key(event, now);
    }

    void wheel(float x, float y, float notches) {
        rummage::ui::PointerEvent event;
        event.kind = rummage::ui::PointerEvent::Kind::scroll;
        event.x = x;
        event.y = y;
        event.wheel_y = notches;
        browser.pointer(event, now);
    }
};

// The offscreen context, or nullptr (and a message) on a machine without OpenGL.
std::unique_ptr<rummage::ui::Offscreen> offscreen() {
    try {
        return std::make_unique<rummage::ui::Offscreen>(kWidth, kHeight);
    } catch (const std::runtime_error& error) {
        MESSAGE("no offscreen OpenGL: " << error.what());
        return nullptr;
    }
}

}  // namespace

TEST_CASE("info pane: a long text file's page scrolls with the wheel over it, not elsewhere") {
    const auto gl = offscreen();
    if (!gl) {
        return;
    }
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);  // long.txt
    REQUIRE(f.settle());
    InfoPane& pane = f.pane();
    REQUIRE(pane.text_page().width > 0.0F);
    CHECK(pane.text_max_offset() > 0.0F);
    CHECK(pane.text_offset() == 0.0F);
    const auto page = pane.text_page();
    const float inside_x = page.x + (page.width / 2.0F);
    // Beside the page, still on the pane (the name and the rows under it): the pane scrolls.
    f.wheel(inside_x, page.bottom() + 10.0F, 3.0F);
    f.draw(1000ms);
    CHECK(pane.text_offset() == 0.0F);

    // Over it: the page. Three notches of 48 logical pixels.
    f.wheel(inside_x, page.y + (page.height / 2.0F), 3.0F);
    f.draw(1000ms);
    CHECK(pane.text_offset() == doctest::Approx(144.0F).epsilon(0.01));
    CHECK(pane.text_offset() <= pane.text_max_offset());
}

TEST_CASE("info pane: Alt+J and Alt+K move the page three lines, Alt+PgDn and Alt+PgUp a page") {
    const auto gl = offscreen();
    if (!gl) {
        return;
    }
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);  // long.txt
    REQUIRE(f.settle());
    InfoPane& pane = f.pane();
    REQUIRE(pane.text_max_offset() > 0.0F);
    const rummage::ui::Modifiers alt{.alt = true};
    const float line = InfoPane::kTextLineHeight;

    f.press(XKB_KEY_j, alt);
    f.draw(1000ms);
    CHECK(pane.text_offset() == doctest::Approx(3.0F * line).epsilon(0.01));
    f.press(XKB_KEY_J, alt);  // Caps Lock
    f.draw(1000ms);
    CHECK(pane.text_offset() == doctest::Approx(6.0F * line).epsilon(0.01));
    f.press(XKB_KEY_k, alt);
    f.draw(1000ms);
    CHECK(pane.text_offset() == doctest::Approx(3.0F * line).epsilon(0.01));

    // A page: what the page shows less a line.
    const float before = pane.text_offset();
    f.press(XKB_KEY_Page_Down, alt);
    f.draw(1000ms);
    const float page = pane.text_offset() - before;
    CHECK(page > 10.0F * line);
    CHECK(page < pane.text_page().height);
    f.press(XKB_KEY_Page_Up, alt);
    f.draw(1000ms);
    CHECK(pane.text_offset() == doctest::Approx(before).epsilon(0.01));

    // Past the top: stays at it.
    f.press(XKB_KEY_Page_Up, alt);
    f.press(XKB_KEY_Page_Up, alt);
    f.draw(1000ms);
    CHECK(pane.text_offset() == 0.0F);
    // And the end.
    for (int i = 0; i < 20; ++i) {
        f.press(XKB_KEY_Page_Down, alt);
    }
    f.draw(2000ms);
    CHECK(pane.text_offset() == pane.text_max_offset());
}

TEST_CASE(
    "info pane: the keys work in a file picker, which has no panes, and not under Quick Look") {
    const auto gl = offscreen();
    if (!gl) {
        return;
    }
    Fixture f;
    f.browser.set_panes_enabled(false);
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);  // long.txt
    REQUIRE(f.settle());
    InfoPane& pane = f.pane();
    const rummage::ui::Modifiers alt{.alt = true};
    f.press(XKB_KEY_j, alt);
    f.draw(1000ms);
    CHECK(pane.text_offset() == doctest::Approx(3.0F * InfoPane::kTextLineHeight).epsilon(0.01));

    // Quick Look covers the pane: the keys are not for it.
    f.press(XKB_KEY_space);
    REQUIRE(f.browser.quick_look().is_open());
    f.press(XKB_KEY_j, alt);
    f.press(XKB_KEY_Page_Down, alt);
    f.draw(1000ms);
    CHECK(pane.text_offset() == doctest::Approx(3.0F * InfoPane::kTextLineHeight).epsilon(0.01));
}

TEST_CASE("info pane: another file starts at the top, and a short one has nothing to scroll") {
    const auto gl = offscreen();
    if (!gl) {
        return;
    }
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);  // long.txt
    REQUIRE(f.settle());
    InfoPane& pane = f.pane();
    const rummage::ui::Modifiers alt{.alt = true};
    f.press(XKB_KEY_j, alt);
    f.draw(1000ms);
    REQUIRE(pane.text_offset() > 0.0F);

    f.press(XKB_KEY_Down);  // short.txt: five lines fit the page
    REQUIRE(f.settle());
    REQUIRE(pane.text_page().width > 0.0F);
    CHECK(pane.text_offset() == 0.0F);
    CHECK(pane.text_max_offset() == 0.0F);
    f.press(XKB_KEY_j, alt);
    f.wheel(pane.text_page().x + 10.0F, pane.text_page().y + 10.0F, 3.0F);
    f.draw(1000ms);
    CHECK(pane.text_offset() == 0.0F);

    f.press(XKB_KEY_Up);  // back to long.txt: at the top, not where it was left
    REQUIRE(f.settle());
    CHECK(pane.text_max_offset() > 0.0F);
    CHECK(pane.text_offset() == 0.0F);
}

TEST_CASE("info pane: a log larger than what is read opens at its newest lines") {
    const auto gl = offscreen();
    if (!gl) {
        return;
    }
    Fixture f;
    {
        std::ofstream log{f.dir.path / "a.log"};
        // More than is read (256 KiB): only then is what is shown the log's end.
        for (int i = 0; i < 12000; ++i) {
            log << "2026-11-13 12:00:" << i << " something happened\n";
        }
    }
    // The initial scan may have completed before the log was created.
    REQUIRE(run_until(f.loop, [&] {
        const auto& entries = f.browser.columns().column(0).folder().entries();
        return std::ranges::any_of(
            entries, [](const rummage::fs::Entry& entry) { return entry.name == "a.log"; });
    }));
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);  // a.log, before long.txt
    REQUIRE(f.settle());
    const auto* selected = f.browser.columns().column(0).folder().cursor_entry();
    REQUIRE(selected != nullptr);
    REQUIRE(selected->name == "a.log");
    InfoPane& pane = f.pane();
    // The log is read off the main thread; under load it can arrive after the folder settles.
    REQUIRE(run_until(f.loop, [&] {
        f.draw();
        return pane.text_page().width > 0.0F;
    }));
    REQUIRE(pane.text_max_offset() > 0.0F);
    CHECK(pane.text_offset() == pane.text_max_offset());
}

TEST_CASE("info pane: replacing or hiding a scrolling text page stops redraws") {
    const auto gl = offscreen();
    if (!gl) {
        return;
    }
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);  // long.txt
    REQUIRE(f.settle());
    f.press(XKB_KEY_j, {.alt = true});
    f.draw(16ms);
    REQUIRE(f.pane().animating());
    SUBCASE("multiple selection") { f.press(XKB_KEY_a, {.ctrl = true}); }
    SUBCASE("hidden pane") { f.browser.set_info_visible(false); }
    f.draw(16ms);
    REQUIRE(f.pane().text_page().width == 0.0F);
    CHECK_FALSE(f.pane().animating());
    f.draw(1000ms);
    CHECK_FALSE(f.browser.animating());
}

TEST_CASE("info pane: text pages fit short windows and reach their final line") {
    const auto gl = offscreen();
    if (!gl) {
        return;
    }
    Fixture f;
    SUBCASE("minimum window height") { f.height = 320; }
    SUBCASE("short window") { f.height = 400; }
    SUBCASE("default window height") { f.height = 640; }
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);  // long.txt
    REQUIRE(f.settle());
    const auto page = f.pane().text_page();
    REQUIRE(page.width > 0.0F);
    CHECK(page.y >= 0.0F);
    CHECK(page.bottom() <= static_cast<float>(f.height));

    f.press(XKB_KEY_Page_Down, {.alt = true});
    f.draw(1000ms);
    CHECK(f.pane().text_offset() > 0.0F);
    CHECK(f.pane().text_offset() < page.height);
    CHECK(f.pane().text_page().y == page.y);  // the outer pane did not move
    for (int i = 0; i < 100; ++i) {
        f.press(XKB_KEY_Page_Down, {.alt = true});
    }
    f.draw(2000ms);
    CHECK(f.pane().text_offset() == f.pane().text_max_offset());
    REQUIRE(f.pane().summary() != nullptr);
    REQUIRE(f.pane().summary()->text.has_value());
    const float content =
        static_cast<float>(f.pane().summary()->text->lines.size()) * InfoPane::kTextLineHeight;
    const float viewport = content - f.pane().text_max_offset();
    CHECK(viewport < page.height);
    CHECK(viewport >= InfoPane::kTextLineHeight);

    // Resizing to a tall window expands the page and clamps the offset at the new end.
    const float previous_max = f.pane().text_max_offset();
    f.height = kHeight;
    f.draw(1000ms);
    CHECK(f.pane().text_max_offset() <= previous_max);
    CHECK(f.pane().text_offset() == f.pane().text_max_offset());
}
