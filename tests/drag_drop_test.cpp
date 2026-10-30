#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <doctest/doctest.h>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/fs/places.hpp"
#include "nordend/fs/trash.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/clipboard.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/offscreen.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/views/browser.hpp"
#include "nordend/views/file_clipboard.hpp"
#include "temp_dir.hpp"

using nordend::core::EventLoop;
using nordend::test::TempDir;
using nordend::ui::DropEvent;
using nordend::ui::KeyEvent;
using nordend::ui::Modifiers;
using nordend::ui::PointerButton;
using nordend::ui::PointerEvent;
using nordend::views::Browser;
using nordend::views::ViewHit;
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

Names listing(const fs::path& directory) {
    Names names;
    std::error_code ignored;
    for (const fs::directory_entry& entry : fs::directory_iterator{directory, ignored}) {
        names.push_back(entry.path().filename().string());
    }
    std::ranges::sort(names);
    return names;
}

KeyEvent key(xkb_keysym_t keysym, Modifiers modifiers = {}) {
    KeyEvent event;
    event.keysym = keysym;
    event.modifiers = modifiers;
    return event;
}

// The GL context the drawing needs; nullopt where the platform has none (the tests then stop).
std::unique_ptr<nordend::ui::Offscreen> offscreen() {
    try {
        return std::make_unique<nordend::ui::Offscreen>(kWidth, kHeight);
    } catch (const std::runtime_error& error) {
        MESSAGE("no offscreen OpenGL: " << error.what());
        return nullptr;
    }
}

// What the app's Wayland side would do, recorded.
struct FakeDnd {
    std::vector<nordend::views::DragAndDrop::Format> started;
    int cancelled{0};
    int finished{0};
    std::vector<std::string> types;
    std::uint32_t source_actions{0};
    std::uint32_t action{0};
    std::optional<std::string> accepted;  // the type taken where the pointer is
    std::uint32_t accepted_actions{0};
    std::string data;  // what a read of another program's drop gives
    Modifiers modifiers;
};

// root/{album/, photos/{P1.ORF, P1.ORF.xmp, P2.ORF}, sub/inner/}, a trash of its own, drawn
// offscreen so the pointer finds what is shown.
struct Fixture {
    TempDir dir;
    fs::path root = dir.path / "root";
    fs::path photos = root / "photos";
    fs::path album = root / "album";
    fs::path trash = dir.path / "Trash";
    std::unique_ptr<nordend::ui::Offscreen> gl = offscreen();
    std::unique_ptr<nordend::ui::FontSystem> fonts;
    std::unique_ptr<nordend::ui::Renderer> renderer;
    std::unique_ptr<nordend::ui::IconSet> icons;
    EventLoop loop;
    nordend::core::WorkerPool pool{loop, 2};
    nordend::fs::DirectoryWatcher watcher{loop};
    Browser browser{loop,    pool,
                    watcher, nordend::fs::MimeDatabase::load(),
                    [] {},   [](nordend::ui::CursorShape /*shape*/) {}};
    FakeDnd dnd;

    Fixture() {
        fs::create_directories(photos);
        fs::create_directories(album);
        fs::create_directories(root / "sub" / "inner");
        for (const char* name : {"P1.ORF", "P1.ORF.xmp", "P2.ORF"}) {
            std::ofstream{photos / name} << name;
        }
        browser.set_home_trash(trash);
        browser.set_places({{.section = nordend::fs::Place::Section::places,
                             .label = "Album",
                             .path = album,
                             .icon = "folder"},
                            {.section = nordend::fs::Place::Section::places,
                             .label = "Trash",
                             .path = nordend::fs::trash_path(),
                             .icon = "trash"}});
        if (gl) {
            fonts = std::make_unique<nordend::ui::FontSystem>();
            renderer = std::make_unique<nordend::ui::Renderer>(*fonts);
            icons = std::make_unique<nordend::ui::IconSet>(*fonts);
        }
        browser.set_drag_and_drop({
            .start =
                [this](std::vector<nordend::views::DragAndDrop::Format> formats) {
                    dnd.started = std::move(formats);
                },
            .cancel = [this] { ++dnd.cancelled; },
            .types = [this] { return dnd.types; },
            .source_actions = [this] { return dnd.source_actions; },
            .action = [this] { return dnd.action; },
            .accept =
                [this](std::optional<std::string> type, std::uint32_t actions,
                       std::uint32_t /*preferred*/) {
                    dnd.accepted = std::move(type);
                    dnd.accepted_actions = actions;
                },
            .read =
                [this](const std::string& /*type*/, nordend::views::DragAndDrop::Done done) {
                    loop.post([this, done = std::move(done)] { done(dnd.data); });
                },
            .finish = [this] { ++dnd.finished; },
            .modifiers = [this] { return dnd.modifiers; },
        });
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

    void show(const fs::path& folder, Browser::Mode mode) {
        browser.open(folder);
        browser.set_mode(mode);
        REQUIRE(settle());
    }

    [[nodiscard]] std::optional<ViewHit> hit(float x, float y) {
        switch (browser.mode()) {
            case Browser::Mode::grid:
                return browser.grid().hit(x, y);
            case Browser::Mode::details:
                return browser.details().hit(x, y);
            case Browser::Mode::columns:
                break;
        }
        return browser.columns().hit(x, y);
    }

    // A point over the entry called name of folder (name empty: over folder's view, off its
    // entries), as last drawn: the middle of the first line of them.
    std::pair<float, float> point_of(const fs::path& folder, const std::string& name) {
        for (float y = 0.0F; y < kHeight; y += 3.0F) {
            std::vector<float> xs;
            for (float x = 0.0F; x < kWidth; x += 10.0F) {
                const std::optional<ViewHit> found = hit(x, y);
                if (!found || found->folder->path() != folder) {
                    continue;
                }
                if (name.empty()
                        ? !found->entry
                        : found->entry && found->folder->entries()[*found->entry].name == name) {
                    xs.push_back(x);
                }
            }
            if (!xs.empty()) {
                return {xs[xs.size() / 2], y + 6.0F};
            }
        }
        FAIL("nothing shows " << (folder / name));
        return {};
    }

    void pointer(PointerEvent::Kind kind, std::pair<float, float> at, int clicks = 1) {
        browser.pointer({.kind = kind,
                         .x = at.first,
                         .y = at.second,
                         .button = PointerButton::left,
                         .click_count = clicks},
                        Browser::Clock::now());
    }

    // Presses on the entry and moves past the threshold: a drag starts.
    void drag(const fs::path& folder, const std::string& name) {
        const auto at = point_of(folder, name);
        pointer(PointerEvent::Kind::motion, at);
        pointer(PointerEvent::Kind::press, at);
        pointer(PointerEvent::Kind::motion, {at.first + 12.0F, at.second + 4.0F});
        REQUIRE(browser.dragging());
    }

    // Moves a drag over the point and drops it there.
    void drop_at(std::pair<float, float> at) {
        browser.drop_event({.kind = DropEvent::Kind::enter, .x = at.first, .y = at.second});
        browser.drop_event({.kind = DropEvent::Kind::motion, .x = at.first, .y = at.second});
        browser.drop_event({.kind = DropEvent::Kind::drop, .x = at.first, .y = at.second});
        browser.drag_ended();
        REQUIRE(settle());
    }
};

}  // namespace

TEST_CASE("drag: a press on an entry moved a few pixels drags the selection") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.show(f.photos, Browser::Mode::details);
    CHECK(f.browser.key(key(XKB_KEY_a, {.ctrl = true}), Browser::Clock::now()));
    f.draw();

    // A click on one of them without moving: that one alone, once the button is up.
    const auto at = f.point_of(f.photos, "P2.ORF");
    f.pointer(PointerEvent::Kind::press, at);
    CHECK(f.browser.details().folder()->selected_count() == 3);
    f.pointer(PointerEvent::Kind::release, at);
    CHECK(f.browser.details().folder()->selected_count() == 1);
    CHECK_FALSE(f.browser.dragging());

    CHECK(f.browser.key(key(XKB_KEY_a, {.ctrl = true}), Browser::Clock::now()));
    f.drag(f.photos, "P2.ORF");
    CHECK(f.browser.details().folder()->selected_count() == 3);
    REQUIRE(f.dnd.started.size() == 3);
    CHECK(f.dnd.started[0].first == "text/uri-list");
    const nordend::views::ClipboardFiles offered =
        nordend::views::parse_uri_list(f.dnd.started[0].second);
    CHECK(offered.paths ==
          std::vector<fs::path>{f.photos / "P1.ORF", f.photos / "P1.ORF.xmp", f.photos / "P2.ORF"});
    CHECK(f.dnd.started[1].second == (f.photos / "P1.ORF").string() + "\n" +
                                         (f.photos / "P1.ORF.xmp").string() + "\n" +
                                         (f.photos / "P2.ORF").string());

    // Escape stops it.
    CHECK(f.browser.key(key(XKB_KEY_Escape), Browser::Clock::now()));
    CHECK(f.dnd.cancelled == 1);
    CHECK_FALSE(f.browser.dragging());
}

TEST_CASE("drop: onto a folder in the columns moves on the same disk, Ctrl copies; Ctrl+Z") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.browser.reveal(f.root, f.photos / "P2.ORF");
    REQUIRE(f.settle());
    f.drag(f.photos, "P2.ORF");
    const auto album = f.point_of(f.root, "album");
    f.browser.drop_event({.kind = DropEvent::Kind::enter, .x = album.first, .y = album.second});
    CHECK(f.dnd.accepted == std::optional<std::string>{"text/uri-list"});
    CHECK(f.browser.drop_mark().directory == f.root);
    CHECK(f.browser.drop_mark().name == "album");
    f.drop_at(album);
    CHECK(f.dnd.finished == 1);
    CHECK(listing(f.album) == Names{"P2.ORF"});
    CHECK(listing(f.photos) == Names{"P1.ORF", "P1.ORF.xmp"});
    CHECK(f.browser.drop_mark().directory.empty());

    f.dnd.modifiers = {.ctrl = true};
    f.drag(f.photos, "P1.ORF");
    f.drop_at(f.point_of(f.root, "album"));
    CHECK(listing(f.album) == Names{"P1.ORF", "P1.ORF.xmp", "P2.ORF"});
    CHECK(listing(f.photos) == Names{"P1.ORF", "P1.ORF.xmp"});

    CHECK(f.browser.key(key(XKB_KEY_z, {.ctrl = true}), Browser::Clock::now()));
    REQUIRE(f.settle());
    CHECK(listing(f.album) == Names{"P2.ORF"});  // the copy went to the trash
}

TEST_CASE("drop: Shift moves, onto the empty part of a view too") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.show(f.root, Browser::Mode::columns);
    f.browser.columns().focused_folder()->select_name("photos");
    REQUIRE(f.settle());
    f.dnd.modifiers = {.shift = true};
    f.drag(f.photos, "P1.ORF");
    f.drop_at(f.point_of(f.root, ""));  // the root's column, below its rows
    CHECK(listing(f.root) == Names{"P1.ORF", "P1.ORF.xmp", "album", "photos", "sub"});
}

TEST_CASE("drop: not into itself, nor where they already are") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.show(f.root, Browser::Mode::details);
    f.drag(f.root, "sub");
    const auto sub = f.point_of(f.root, "sub");
    f.browser.drop_event({.kind = DropEvent::Kind::enter, .x = sub.first, .y = sub.second});
    CHECK_FALSE(f.dnd.accepted);
    CHECK(f.browser.drop_mark().directory.empty());
    const auto here = f.point_of(f.root, "");
    f.browser.drop_event({.kind = DropEvent::Kind::motion, .x = here.first, .y = here.second});
    CHECK_FALSE(f.dnd.accepted);
    const auto album = f.point_of(f.root, "album");
    f.browser.drop_event({.kind = DropEvent::Kind::motion, .x = album.first, .y = album.second});
    CHECK(f.dnd.accepted);
    f.browser.drop_event({.kind = DropEvent::Kind::leave, .x = album.first, .y = album.second});
    CHECK(f.browser.drop_mark().directory.empty());
    f.drop_at(sub);  // dropped there all the same: nothing happens
    CHECK(listing(f.root / "sub") == Names{"inner"});
}

TEST_CASE("drop: from another program, its files are copied; web addresses are not files") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    const fs::path outside = f.dir.path / "Download dir";
    fs::create_directories(outside);
    std::ofstream{outside / "naïve photo.jpg"} << "jpeg";
    f.show(f.root, Browser::Mode::grid);
    f.dnd.types = {"text/uri-list", "text/plain"};
    f.dnd.source_actions = nordend::ui::kDndCopy;
    f.dnd.action = nordend::ui::kDndCopy;
    f.dnd.data = nordend::views::uri_list(std::vector<fs::path>{outside / "naïve photo.jpg"}) +
                 "https://example.com/a.jpg\r\n";
    const auto album = f.point_of(f.root, "album");
    f.browser.drop_event({.kind = DropEvent::Kind::enter, .x = album.first, .y = album.second});
    CHECK(f.dnd.accepted == std::optional<std::string>{"text/uri-list"});
    CHECK(f.dnd.accepted_actions == nordend::ui::kDndCopy);  // the source allows nothing else
    f.drop_at(album);
    CHECK(listing(f.album) == Names{"naïve photo.jpg"});
    CHECK(listing(outside) == Names{"naïve photo.jpg"});

    f.dnd.data = "https://example.com/a.jpg\r\n";
    f.drop_at(f.point_of(f.root, "sub"));
    CHECK(f.browser.status_bar().message() == "Only files can be dropped");

    // Without files in it, a drag finds nowhere to go.
    f.dnd.types = {"text/plain"};
    f.dnd.accepted.reset();
    f.browser.drop_event({.kind = DropEvent::Kind::enter, .x = album.first, .y = album.second});
    CHECK_FALSE(f.dnd.accepted);
}

TEST_CASE("drop: onto a step of the breadcrumb and a place of the sidebar; the trash trashes") {
    Fixture f;
    if (!f.gl) {
        return;
    }
    f.show(f.photos, Browser::Mode::details);
    // The breadcrumb's step for root.
    std::optional<std::pair<float, float>> crumb;
    for (float x = 200.0F; x < kWidth && !crumb; x += 4.0F) {
        if (f.browser.breadcrumb().path_at(x, 20.0F) == f.root) {
            crumb = std::pair{x, 20.0F};
        }
    }
    REQUIRE(crumb);
    f.drag(f.photos, "P2.ORF");
    f.drop_at(crumb.value_or(std::pair{0.0F, 0.0F}));
    CHECK(listing(f.root) == Names{"P2.ORF", "album", "photos", "sub"});

    // The sidebar's places: Album, then the trash.
    const auto place = [&f](const fs::path& path) {
        for (float y = 0.0F; y < kHeight; y += 3.0F) {
            if (const nordend::fs::Place* found = f.browser.sidebar().place_under(40.0F, y);
                found != nullptr && found->path == path) {
                return std::pair{40.0F, y};
            }
        }
        FAIL("no place for " << path);
        return std::pair{0.0F, 0.0F};
    };
    f.drag(f.photos, "P1.ORF");
    f.drop_at(place(f.album));
    CHECK(listing(f.album) == Names{"P1.ORF", "P1.ORF.xmp"});

    f.show(f.root, Browser::Mode::details);
    f.drag(f.root, "P2.ORF");
    f.drop_at(place(nordend::fs::trash_path()));
    CHECK(listing(f.trash / "files") == Names{"P2.ORF"});
}
