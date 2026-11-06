#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <doctest/doctest.h>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/offscreen.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/views/browser.hpp"
#include "temp_dir.hpp"

using nordend::core::EventLoop;
using nordend::test::TempDir;
using nordend::ui::CursorShape;
using nordend::ui::KeyEvent;
using nordend::ui::Modifiers;
using nordend::ui::PointerButton;
using nordend::ui::PointerEvent;
using nordend::views::Browser;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

using Names = std::vector<std::string>;

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

constexpr Modifiers kCtrl{.ctrl = true};
constexpr Modifiers kCtrlShift{.ctrl = true, .shift = true};
constexpr Modifiers kAlt{.alt = true};

std::unique_ptr<nordend::ui::Offscreen> offscreen() {
    try {
        return std::make_unique<nordend::ui::Offscreen>(kWidth, kHeight);
    } catch (const std::runtime_error& error) {
        MESSAGE("no offscreen OpenGL: " << error.what());
        return nullptr;
    }
}

// a/{one.txt, sub/}, b/{two.txt}, in the columns at a.
struct Fixture {
    TempDir dir;
    fs::path a = dir.path / "a";
    fs::path b = dir.path / "b";
    EventLoop loop;
    nordend::core::WorkerPool pool{loop, 2};
    nordend::fs::DirectoryWatcher watcher{loop};
    Browser browser{loop,    pool,
                    watcher, nordend::fs::MimeDatabase::load(),
                    [] {},   [](CursorShape /*shape*/) {}};

    Fixture() {
        fs::create_directories(a / "sub");
        fs::create_directories(b);
        std::ofstream{a / "one.txt"} << "1";
        std::ofstream{b / "two.txt"} << "2";
        browser.set_home_trash(dir.path / "Trash");
        browser.open(a);
        REQUIRE(settle());
    }

    bool settle() {
        return run_until(loop, [this] { return !browser.loading(); });
    }

    bool press(const KeyEvent& event) {
        const bool handled = browser.key(event, Browser::Clock::now());
        REQUIRE(settle());
        return handled;
    }
};

}  // namespace

TEST_CASE("tabs: Ctrl+T, Ctrl+Tab, Ctrl+Shift+Tab, Alt+1…9, Ctrl+W; the last tab stays") {
    Fixture f;
    CHECK(f.browser.tab_count() == 1);
    CHECK(f.press(key(XKB_KEY_w, kCtrl)));  // the last tab: nothing
    CHECK(f.browser.tab_count() == 1);

    // A new tab on the folder shown, shown at once.
    CHECK(f.press(key(XKB_KEY_t, kCtrl)));
    CHECK(f.browser.tab_count() == 2);
    CHECK(f.browser.active_tab() == 1);
    CHECK(f.browser.tab_titles() == Names{"a", "a"});
    f.browser.open(f.b);
    REQUIRE(f.settle());
    CHECK(f.browser.tab_titles() == Names{"a", "b"});

    // Round and round.
    CHECK(f.press(key(XKB_KEY_Tab, kCtrl)));
    CHECK(f.browser.active_tab() == 0);
    CHECK(f.browser.columns().focused_folder()->path() == f.a);
    CHECK(f.press(key(XKB_KEY_ISO_Left_Tab, kCtrlShift)));
    CHECK(f.browser.active_tab() == 1);
    CHECK(f.press(key(XKB_KEY_Tab, kCtrl)));
    CHECK(f.browser.active_tab() == 0);
    CHECK(f.press(key(XKB_KEY_2, kAlt)));
    CHECK(f.browser.active_tab() == 1);
    CHECK(f.press(key(XKB_KEY_9, kAlt)));  // no ninth tab: nothing
    CHECK(f.browser.active_tab() == 1);

    // Closing the tab shown: the one before comes when it was the last.
    CHECK(f.press(key(XKB_KEY_w, kCtrl)));
    CHECK(f.browser.tab_count() == 1);
    CHECK(f.browser.active_tab() == 0);
    CHECK(f.browser.tab_titles() == Names{"a"});
}

TEST_CASE("tabs: each has its view and history; the order and hidden files are the window's") {
    Fixture f;
    f.browser.set_mode(Browser::Mode::grid);
    CHECK(f.press(key(XKB_KEY_t, kCtrl)));  // in the grid too
    CHECK(f.browser.mode() == Browser::Mode::grid);
    f.browser.open(f.b);
    REQUIRE(f.settle());
    f.browser.set_mode(Browser::Mode::details);
    CHECK(f.browser.history().size() == 2);  // a, then b

    f.browser.select_tab(0);
    CHECK(f.browser.mode() == Browser::Mode::grid);
    CHECK(f.browser.history().size() == 1);
    CHECK(f.browser.grid().folder()->path() == f.a);

    // The window's order and hidden files reach every tab, those made later too.
    f.browser.set_sort(nordend::fs::SortKey::size, true);
    f.browser.set_show_hidden(true);
    f.browser.select_tab(1);
    CHECK(f.browser.sort_order().key == nordend::fs::SortKey::size);
    CHECK(f.browser.sort_order().descending);
    CHECK(f.browser.details().folder()->order().key == nordend::fs::SortKey::size);
    CHECK(f.press(key(XKB_KEY_t, kCtrl)));
    CHECK(f.browser.details().order().key == nordend::fs::SortKey::size);
    CHECK(f.browser.details().folder()->order().descending);

    // Back in the second tab goes through its own history only.
    f.browser.select_tab(1);
    CHECK(f.press(key(XKB_KEY_Left, kAlt)));
    CHECK(f.browser.details().folder()->path() == f.a);
    f.browser.select_tab(0);
    CHECK(f.browser.history_index() == 0);
}

TEST_CASE("tabs: opened behind the one shown, right after it; a closed tab gives its place") {
    Fixture f;
    f.browser.open_tab(f.b, false);
    REQUIRE(f.settle());
    CHECK(f.browser.tab_count() == 2);
    CHECK(f.browser.active_tab() == 0);  // still here
    CHECK(f.browser.status_bar().message() == "Opened “b” in a new tab");
    f.browser.open_tab(f.a / "sub", false);
    REQUIRE(f.settle());
    CHECK(f.browser.tab_titles() == Names{"a", "sub", "b"});
    CHECK(f.browser.active_tab() == 0);

    // Closing the tab shown: the one after it takes its place.
    f.browser.select_tab(1);
    f.browser.close_tab(1);
    CHECK(f.browser.tab_titles() == Names{"a", "b"});
    CHECK(f.browser.active_tab() == 1);
    // A tab before the one shown: the one shown stays.
    f.browser.close_tab(0);
    CHECK(f.browser.tab_titles() == Names{"b"});
    CHECK(f.browser.active_tab() == 0);
    CHECK(f.browser.columns().focused_folder()->path() == f.b);
}

TEST_CASE("tabs: Open in New Tab, one folder or several, behind the tab shown") {
    Fixture f;
    fs::create_directories(f.a / "more");
    REQUIRE(run_until(f.loop, [&f] {
        const auto& entries = f.browser.columns().focused_folder()->entries();
        return std::ranges::find(entries, "more", &nordend::fs::Entry::name) != entries.end();
    }));
    const auto select = [&f](const Names& names) {
        const std::shared_ptr<nordend::views::Folder> folder = f.browser.columns().focused_folder();
        folder->set_cursor(std::nullopt);
        for (const std::string& name : names) {
            const auto& entries = folder->entries();
            const auto it = std::ranges::find(entries, name, &nordend::fs::Entry::name);
            REQUIRE(it != entries.end());
            folder->set_cursor(static_cast<std::size_t>(it - entries.begin()),
                               nordend::views::SelectMode::toggle);
        }
    };
    // The label of the menu's item called id (empty: none), and choosing it.
    const auto label = [&f](const std::string& id) {
        for (const nordend::views::MenuItem& each : f.browser.context_menu().items()) {
            if (each.id == id) {
                return each.label;
            }
        }
        return std::string{};
    };
    const auto choose = [&f](const std::string& id) {
        std::function<void()> action;
        for (const nordend::views::MenuItem& each : f.browser.context_menu().items()) {
            if (each.id == id) {
                action = each.action;
            }
        }
        f.browser.context_menu().close();
        REQUIRE(action);
        action();
        REQUIRE(f.settle());
    };

    // A file: no such item. A folder: one tab.
    select({"one.txt"});
    f.browser.open_menu(100, 100, false);
    CHECK(label("open_tab").empty());
    f.browser.context_menu().close();
    select({"sub"});
    f.browser.open_menu(100, 100, false);
    CHECK(label("open_tab") == "Open in New Tab");
    choose("open_tab");
    CHECK(f.browser.tab_titles() == Names{"a", "sub"});
    CHECK(f.browser.active_tab() == 0);

    // Two folders: two tabs, in their order, right after the one shown.
    select({"more", "sub"});
    f.browser.open_menu(100, 100, false);
    CHECK(label("open_tab") == "Open in New Tabs");
    choose("open_tab");
    CHECK(f.browser.tab_titles() == Names{"a", "more", "sub", "sub"});
    CHECK(f.browser.active_tab() == 0);
    CHECK(f.browser.status_bar().message() == "Opened 2 folders in new tabs");
}

TEST_CASE("tabs: closed while a name is edited, the rename still lands") {
    Fixture f;
    CHECK(f.press(key(XKB_KEY_t, kCtrl)));
    CHECK(f.press(key(XKB_KEY_Down)));  // the cursor on sub (folders first)
    CHECK(f.press(key(XKB_KEY_F2)));
    REQUIRE(f.browser.name_editor().active());
    f.browser.key(key(XKB_KEY_End), Browser::Clock::now());
    KeyEvent typed = key(XKB_KEY_2);
    typed.text = "2";
    f.browser.key(typed, Browser::Clock::now());
    // Ctrl+W is the field's meanwhile; the tab's × closes it, keeping the name typed. Its columns
    // go with it: the folder, held by the rename, reports to nothing.
    f.browser.key(key(XKB_KEY_w, kCtrl), Browser::Clock::now());
    CHECK(f.browser.tab_count() == 2);
    f.browser.close_tab(1);
    CHECK(f.browser.tab_count() == 1);
    REQUIRE(run_until(f.loop, [&f] { return fs::exists(f.a / "sub2"); }));
    REQUIRE(f.settle());
    CHECK(f.browser.columns().focused_folder()->path() == f.a);

    // A folder kept past its tab: moving its cursor tells no columns (ASan would see them gone).
    CHECK(f.press(key(XKB_KEY_t, kCtrl)));
    const std::shared_ptr<nordend::views::Folder> held = f.browser.columns().focused_folder();
    f.browser.close_tab(1);
    held->set_cursor(0);
    CHECK(held->cursor() == 0);
}

TEST_CASE("tabs: the bar with two tabs; a click shows one, its × or a middle click closes it") {
    Fixture f;
    const std::unique_ptr<nordend::ui::Offscreen> gl = offscreen();
    if (!gl) {
        return;
    }
    nordend::ui::FontSystem fonts;
    nordend::ui::Renderer renderer{fonts};
    nordend::ui::IconSet icons{fonts};
    const auto draw = [&] {
        renderer.begin_frame(kWidth, kHeight, 1.0F, {});
        f.browser.draw(renderer, {0.0F, 0.0F, kWidth, kHeight}, nordend::theme::Palette{}, icons,
                       Browser::Clock::now());
        renderer.end_frame();
    };
    const auto click = [&f](const nordend::ui::Rect& rect, PointerButton button) {
        const float x = rect.x + (rect.width / 2.0F);
        const float y = rect.y + (rect.height / 2.0F);
        f.browser.pointer({.kind = PointerEvent::Kind::motion, .x = x, .y = y},
                          Browser::Clock::now());
        f.browser.pointer(
            {.kind = PointerEvent::Kind::press, .x = x, .y = y, .button = button, .click_count = 1},
            Browser::Clock::now());
        f.browser.pointer({.kind = PointerEvent::Kind::release, .x = x, .y = y, .button = button},
                          Browser::Clock::now());
    };
    const nordend::views::TabBar& bar = f.browser.tab_bar();

    draw();
    CHECK(bar.tab_rects().empty());  // one tab: no bar
    CHECK(f.press(key(XKB_KEY_t, kCtrl)));
    draw();
    REQUIRE(bar.tab_rects().size() == 2);
    const nordend::ui::Rect first = bar.tab_rects()[0];
    CHECK(f.browser.cursor_at(first.x + 20.0F, first.y + 10.0F) == CursorShape::pointer);
    // The path and the view moved down under the bar.
    CHECK(f.browser.breadcrumb().claims(first.x + 20.0F, first.bottom() + 5.0F));

    click(first, PointerButton::left);
    CHECK(f.browser.active_tab() == 0);
    draw();
    click(bar.close_rects()[1], PointerButton::left);  // the second tab's ×
    CHECK(f.browser.tab_count() == 1);
    draw();
    CHECK(bar.tab_rects().empty());

    CHECK(f.press(key(XKB_KEY_t, kCtrl)));
    draw();
    click(bar.tab_rects()[0], PointerButton::middle);
    CHECK(f.browser.tab_count() == 1);
    CHECK(f.browser.active_tab() == 0);

    CHECK(f.press(key(XKB_KEY_t, kCtrl)));
    draw();
    click(bar.add_rect(), PointerButton::left);  // the +
    CHECK(f.browser.tab_count() == 3);
    CHECK(f.browser.active_tab() == 2);

    // A middle click on a folder in a view: a tab behind; on a file, nothing.
    f.browser.close_tab(2);
    f.browser.close_tab(1);
    f.browser.set_mode(Browser::Mode::details);
    REQUIRE(f.settle());
    draw();
    const auto row_of = [&f](const std::string& name) -> std::optional<nordend::ui::Rect> {
        for (float y = 1.0F; y < kHeight; y += 2.0F) {
            for (float x = 300.0F; x < kWidth; x += 40.0F) {
                if (const auto hit = f.browser.details().hit(x, y);
                    hit && hit->entry && hit->folder->entries()[*hit->entry].name == name) {
                    return nordend::ui::Rect{x, y, 1.0F, 1.0F};
                }
            }
        }
        return std::nullopt;
    };
    const std::optional<nordend::ui::Rect> sub = row_of("sub");
    const std::optional<nordend::ui::Rect> one = row_of("one.txt");
    REQUIRE(sub);
    REQUIRE(one);
    click(one.value_or(nordend::ui::Rect{}), PointerButton::middle);
    CHECK(f.browser.tab_count() == 1);
    click(sub.value_or(nordend::ui::Rect{}), PointerButton::middle);
    REQUIRE(f.settle());
    CHECK(f.browser.tab_titles() == Names{"a", "sub"});
    CHECK(f.browser.active_tab() == 0);
    CHECK(f.browser.details().folder()->path() == f.a);  // still here

    // On a folder of the selection: every selected folder, files left out.
    f.browser.close_tab(1);
    fs::create_directories(f.a / "more");
    REQUIRE(run_until(f.loop, [&f] {
        const auto& entries = f.browser.details().folder()->entries();
        return std::ranges::find(entries, "more", &nordend::fs::Entry::name) != entries.end();
    }));
    REQUIRE(f.settle());
    draw();
    const std::shared_ptr<nordend::views::Folder> shown = f.browser.details().folder();
    shown->set_cursor(std::nullopt);
    for (const char* name : {"more", "sub", "one.txt"}) {
        const auto& entries = shown->entries();
        const auto it = std::ranges::find(entries, std::string{name}, &nordend::fs::Entry::name);
        REQUIRE(it != entries.end());
        shown->set_cursor(static_cast<std::size_t>(it - entries.begin()),
                          nordend::views::SelectMode::toggle);
    }
    draw();
    const std::optional<nordend::ui::Rect> selected = row_of("sub");
    REQUIRE(selected);
    click(selected.value_or(nordend::ui::Rect{}), PointerButton::middle);
    REQUIRE(f.settle());
    CHECK(f.browser.tab_titles() == Names{"a", "more", "sub"});
    CHECK(f.browser.active_tab() == 0);
}
