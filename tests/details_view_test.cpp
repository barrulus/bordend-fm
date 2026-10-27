#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <doctest/doctest.h>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/views/browser.hpp"
#include "temp_dir.hpp"

using nordend::core::EventLoop;
using nordend::core::WorkerPool;
using nordend::fs::DirectoryWatcher;
using nordend::fs::SortKey;
using nordend::test::TempDir;
using nordend::views::Browser;
using nordend::views::DetailsView;
using nordend::views::Folder;
using Field = DetailsView::Field;
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

void write(const fs::path& file, std::size_t bytes) {
    std::ofstream{file} << std::string(bytes, 'x');
}

std::string cursor_name(const Folder& folder) {
    const nordend::fs::Entry* entry = folder.cursor_entry();
    return entry == nullptr ? std::string{} : entry->name;
}

std::vector<std::string> names(const Folder& folder) {
    std::vector<std::string> out;
    for (const nordend::fs::Entry& entry : folder.entries()) {
        out.push_back(entry.name);
    }
    return out;
}

// root/{alpha/{inner.txt}, gamma/, a.md (10 bytes), b.txt (100), c.txt (1)}
struct Fixture {
    TempDir dir;
    fs::path root = dir.path / "root";
    EventLoop loop;
    WorkerPool pool{loop, 2};
    DirectoryWatcher watcher{loop};
    Browser browser{loop,    pool,
                    watcher, nordend::fs::MimeDatabase::load(),
                    [] {},   [](nordend::ui::CursorShape /*shape*/) {}};
    DetailsView& details = browser.details();

    Fixture() {
        fs::create_directories(root / "alpha");
        fs::create_directories(root / "gamma");
        write(root / "alpha" / "inner.txt", 1);
        write(root / "a.md", 10);
        write(root / "b.txt", 100);
        write(root / "c.txt", 1);
        browser.open(root);
    }

    bool settle() {
        return run_until(loop, [this] { return !browser.loading(); });
    }

    void press(xkb_keysym_t key, nordend::ui::Modifiers modifiers = {}) {
        nordend::ui::KeyEvent event;
        event.keysym = key;
        event.modifiers = modifiers;
        browser.key(event, Browser::Clock::now());
    }
};

}  // namespace

TEST_CASE("details: Ctrl+3 shows the same folder, cursor and selection; every view switches") {
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);                   // alpha
    f.press(XKB_KEY_Down, {.shift = true});  // alpha, gamma
    const std::shared_ptr<Folder> folder = f.browser.columns().focused_folder();

    f.press(XKB_KEY_3, {.ctrl = true});
    REQUIRE(f.browser.mode() == Browser::Mode::details);
    CHECK(f.details.folder() == folder);  // the same object: nothing reloads
    CHECK(cursor_name(*folder) == "gamma");
    CHECK(folder->selected_names() == std::vector<std::string>{"alpha", "gamma"});

    f.press(XKB_KEY_Down);  // a.md, alone
    CHECK(folder->selected_names() == std::vector<std::string>{"a.md"});
    f.press(XKB_KEY_2, {.ctrl = true});  // straight to the grid
    REQUIRE(f.browser.mode() == Browser::Mode::grid);
    CHECK(f.browser.grid().folder() == folder);
    f.press(XKB_KEY_3, {.ctrl = true});  // and back
    CHECK(f.details.folder() == folder);
    CHECK(cursor_name(*folder) == "a.md");
    f.press(XKB_KEY_1, {.ctrl = true});
    REQUIRE(f.browser.mode() == Browser::Mode::columns);
    CHECK(f.browser.columns().focused_folder() == folder);
}

TEST_CASE("details: sorting by a column, again reversed; the cursor stays on its entry") {
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_3, {.ctrl = true});
    const std::shared_ptr<Folder> folder = f.details.folder();
    REQUIRE(folder != nullptr);
    CHECK(names(*folder) == std::vector<std::string>{"alpha", "gamma", "a.md", "b.txt", "c.txt"});
    f.press(XKB_KEY_End);  // c.txt

    f.details.sort_by(Field::size);
    CHECK(folder->order().key == SortKey::size);
    CHECK_FALSE(folder->order().descending);
    // Folders first, then 1, 10 and 100 bytes.
    CHECK(names(*folder) == std::vector<std::string>{"alpha", "gamma", "c.txt", "a.md", "b.txt"});
    CHECK(cursor_name(*folder) == "c.txt");
    CHECK(folder->cursor() == 2);

    f.details.sort_by(Field::size);  // the same title again: largest first
    CHECK(folder->order().descending);
    CHECK(names(*folder) == std::vector<std::string>{"gamma", "alpha", "b.txt", "a.md", "c.txt"});
    CHECK(cursor_name(*folder) == "c.txt");

    f.details.sort_by(Field::type);  // text/markdown before text/plain
    CHECK(names(*folder) == std::vector<std::string>{"alpha", "gamma", "a.md", "b.txt", "c.txt"});

    // The columns show the folder as sorted; a folder the details open is sorted the same way.
    f.details.sort_by(Field::modified);
    f.press(XKB_KEY_Home);  // alpha or gamma: either is a folder
    f.press(XKB_KEY_Right);
    REQUIRE(f.settle());
    CHECK(f.details.folder()->order().key == SortKey::modified);
    f.press(XKB_KEY_1, {.ctrl = true});
    CHECK(f.browser.columns().focused_folder()->order().key == SortKey::modified);
}

TEST_CASE("details: Right and Enter open a folder, Left and Backspace the parent") {
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_3, {.ctrl = true});
    f.press(XKB_KEY_Down);  // alpha
    f.press(XKB_KEY_Right);
    REQUIRE(f.settle());
    REQUIRE(f.details.folder() != nullptr);
    CHECK(f.details.folder()->path() == f.root / "alpha");
    CHECK_FALSE(f.details.folder()->cursor());  // a folder opens with no cursor

    f.press(XKB_KEY_Left);
    REQUIRE(f.settle());
    CHECK(f.details.folder()->path() == f.root);
    CHECK(cursor_name(*f.details.folder()) == "alpha");  // where we were

    f.press(XKB_KEY_Return);
    REQUIRE(f.settle());
    CHECK(f.details.folder()->path() == f.root / "alpha");
    f.press(XKB_KEY_BackSpace);
    REQUIRE(f.settle());
    CHECK(f.details.folder()->path() == f.root);
    // Default widths before any layout; the name gets what is left once drawn.
    CHECK(f.details.width(Field::size) > 0.0F);
}

TEST_CASE("info pane: Ctrl+I hides and shows it in every view; nothing else moves") {
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);                   // alpha
    f.press(XKB_KEY_Down, {.shift = true});  // alpha, gamma
    const std::shared_ptr<Folder> folder = f.browser.columns().focused_folder();
    CHECK(f.browser.info_visible());  // shown at start

    f.press(XKB_KEY_i, {.ctrl = true});
    CHECK_FALSE(f.browser.info_visible());
    CHECK_FALSE(f.browser.columns().info_visible());
    CHECK_FALSE(f.browser.grid().info_visible());
    CHECK_FALSE(f.details.info_visible());
    CHECK(cursor_name(*folder) == "gamma");
    CHECK(folder->selected_names() == std::vector<std::string>{"alpha", "gamma"});

    f.press(XKB_KEY_3, {.ctrl = true});  // the setting goes along
    CHECK_FALSE(f.details.info_visible());
    f.press(XKB_KEY_I, {.ctrl = true});  // the keysym with Caps Lock on
    CHECK(f.browser.info_visible());
    CHECK(f.details.info_visible());
    CHECK(f.details.folder() == folder);
    CHECK(folder->selected_names() == std::vector<std::string>{"alpha", "gamma"});
}
