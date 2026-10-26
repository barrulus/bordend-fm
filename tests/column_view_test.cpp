#include "nordend/views/column_view.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <doctest/doctest.h>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "temp_dir.hpp"

using nordend::core::EventLoop;
using nordend::core::WorkerPool;
using nordend::fs::DirectoryWatcher;
using nordend::test::TempDir;
using nordend::views::Column;
using nordend::views::ColumnView;
using nordend::views::SelectMode;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

// Runs the loop until done() holds or 5 s pass.
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

void write(const fs::path& file, std::string_view text) { std::ofstream{file} << text; }

std::string cursor_name(Column& column) {
    const nordend::fs::Entry* entry = column.cursor_entry();
    return entry == nullptr ? std::string{} : entry->name;
}

// root/{alpha/{beta/{photo.jpg}, notes.txt}, gamma/, readme.md}
struct Fixture {
    TempDir dir;
    fs::path root = dir.path / "root";
    EventLoop loop;
    WorkerPool pool{loop, 2};
    DirectoryWatcher watcher{loop};
    ColumnView view{loop,    pool,
                    watcher, nordend::fs::MimeDatabase::load(),
                    [] {},   [](nordend::ui::CursorShape /*shape*/) {}};

    Fixture() {
        fs::create_directories(root / "alpha" / "beta");
        fs::create_directories(root / "gamma");
        write(root / "alpha" / "beta" / "photo.jpg", "x");
        write(root / "alpha" / "notes.txt", "notes");
        write(root / "readme.md", "# hi");
    }

    bool settle() {
        return run_until(loop, [this] { return !view.loading(); });
    }

    void press(xkb_keysym_t key, nordend::ui::Modifiers modifiers = {}) {
        nordend::ui::KeyEvent event;
        event.keysym = key;
        event.modifiers = modifiers;
        view.key(event, ColumnView::Clock::now());
    }
};

}  // namespace

TEST_CASE("column view: arrows move the cursor and open folders in the next column") {
    Fixture f;
    f.view.open(f.root);
    REQUIRE(f.settle());
    CHECK(f.view.column_count() == 1);

    f.press(XKB_KEY_Down);  // folders first: alpha
    CHECK(cursor_name(f.view.column(0)) == "alpha");
    REQUIRE(f.view.column_count() == 2);
    CHECK(f.view.column(1).path() == f.root / "alpha");
    REQUIRE(f.settle());

    f.press(XKB_KEY_Right);  // into alpha: the cursor goes to its first entry, beta
    CHECK(f.view.focus() == 1);
    CHECK(cursor_name(f.view.column(1)) == "beta");
    REQUIRE(f.view.column_count() == 3);
    REQUIRE(f.settle());

    f.press(XKB_KEY_Down);  // a file: no column after it
    CHECK(cursor_name(f.view.column(1)) == "notes.txt");
    CHECK(f.view.column_count() == 2);

    f.press(XKB_KEY_Left);  // back: alpha stays open, without a cursor
    CHECK(f.view.focus() == 0);
    CHECK(f.view.column_count() == 2);
    CHECK_FALSE(f.view.column(1).cursor());

    f.press(XKB_KEY_End);  // readme.md
    CHECK(cursor_name(f.view.column(0)) == "readme.md");
    CHECK(f.view.column_count() == 1);
    f.press(XKB_KEY_Up);  // gamma
    CHECK(f.view.column_count() == 2);
    CHECK(f.view.column(1).path() == f.root / "gamma");
}

TEST_CASE("column view: left from the first column opens the parent, cursor on where we were") {
    Fixture f;
    f.view.open(f.root / "alpha");
    REQUIRE(f.settle());
    f.press(XKB_KEY_Left);
    REQUIRE(f.settle());
    REQUIRE(f.view.column_count() == 2);
    CHECK(f.view.focus() == 0);
    CHECK(f.view.column(0).path() == f.root);
    CHECK(cursor_name(f.view.column(0)) == "alpha");
    CHECK(f.view.column(1).path() == f.root / "alpha");
}

TEST_CASE("column view: reveal unfolds the columns down to a file") {
    Fixture f;
    f.view.reveal(f.root, f.root / "alpha" / "beta" / "photo.jpg");
    REQUIRE(f.settle());
    REQUIRE(f.view.column_count() == 3);
    CHECK(cursor_name(f.view.column(0)) == "alpha");
    CHECK(cursor_name(f.view.column(1)) == "beta");
    CHECK(cursor_name(f.view.column(2)) == "photo.jpg");
    CHECK(f.view.focus() == 2);
}

TEST_CASE("column view: a folder deleted or renamed under the cursor updates the columns") {
    Fixture f;
    f.view.reveal(f.root, f.root / "alpha" / "beta");
    REQUIRE(f.settle());
    REQUIRE(f.view.column_count() == 3);

    // Renamed: the cursor follows it, the next column shows it under its new name.
    fs::rename(f.root / "alpha" / "beta", f.root / "alpha" / "delta");
    CHECK(run_until(f.loop, [&] {
        return f.view.column_count() == 3 && f.view.column(2).path() == f.root / "alpha" / "delta";
    }));
    CHECK(cursor_name(f.view.column(1)) == "delta");

    // Deleted: the cursor goes, and the column with it.
    fs::remove_all(f.root / "alpha" / "delta");
    CHECK(run_until(f.loop, [&] { return f.view.column_count() == 2; }));
    CHECK_FALSE(f.view.column(1).cursor());
}

TEST_CASE("column view: Shift extends the selection, which closes the next column") {
    Fixture f;
    f.view.open(f.root);
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);  // alpha, opens its column
    REQUIRE(f.view.column_count() == 2);

    f.press(XKB_KEY_Down, {.shift = true});  // alpha, gamma
    Column& column = f.view.column(0);
    CHECK(column.selected_count() == 2);
    CHECK(cursor_name(column) == "gamma");
    CHECK(f.view.column_count() == 1);  // several entries: nothing to show on the right
    CHECK(column.single_selection() == nullptr);

    f.press(XKB_KEY_End, {.shift = true});  // from the anchor (alpha) to readme.md
    CHECK(column.selected_names() == std::vector<std::string>{"alpha", "gamma", "readme.md"});
    const nordend::views::SelectionSummary& summary = column.summary();
    CHECK(summary.folders == 2);
    CHECK(summary.files == 1);
    CHECK(summary.bytes == 4);  // "# hi"

    f.press(XKB_KEY_Up, {.shift = true});  // the range shrinks back: alpha, gamma
    CHECK(column.selected_names() == std::vector<std::string>{"alpha", "gamma"});

    f.press(XKB_KEY_Escape);  // back to the cursor's entry alone: gamma opens again
    CHECK(column.selected_names() == std::vector<std::string>{"gamma"});
    REQUIRE(f.view.column_count() == 2);
    CHECK(f.view.column(1).path() == f.root / "gamma");
}

TEST_CASE("column view: Ctrl moves the cursor alone, Ctrl+Space toggles, Ctrl+A selects all") {
    Fixture f;
    f.view.open(f.root);
    REQUIRE(f.settle());
    Column& column = f.view.column(0);
    f.press(XKB_KEY_Down);                  // alpha
    f.press(XKB_KEY_Down, {.ctrl = true});  // cursor on gamma, alpha selected
    f.press(XKB_KEY_Down, {.ctrl = true});  // cursor on readme.md
    CHECK(column.selected_names() == std::vector<std::string>{"alpha"});
    CHECK(cursor_name(column) == "readme.md");
    CHECK(f.view.column_count() == 1);  // the cursor is not the selection

    f.press(XKB_KEY_space, {.ctrl = true});  // alpha, readme.md
    CHECK(column.selected_names() == std::vector<std::string>{"alpha", "readme.md"});
    f.press(XKB_KEY_space, {.ctrl = true});  // and off again
    CHECK(column.selected_names() == std::vector<std::string>{"alpha"});
    f.press(XKB_KEY_space);  // plain Space is not selection
    CHECK(column.selected_count() == 1);

    f.press(XKB_KEY_a, {.ctrl = true});
    CHECK(column.selected_count() == 3);
    f.press(XKB_KEY_Right);  // several selected: nothing to enter
    CHECK(f.view.focus() == 0);

    f.press(XKB_KEY_Down);  // a plain arrow selects one entry again
    CHECK(column.selected_names() == std::vector<std::string>{"readme.md"});
}

TEST_CASE("column: toggling and ranges by row, selection follows renames and deletions") {
    Fixture f;
    f.view.open(f.root);
    REQUIRE(f.settle());
    Column& column = f.view.column(0);
    const auto now = Column::Clock::now();
    column.set_cursor(0, now, SelectMode::toggle);  // alpha
    column.set_cursor(2, now, SelectMode::toggle);  // readme.md
    CHECK(column.selected_names() == std::vector<std::string>{"alpha", "readme.md"});
    column.set_cursor(1, now, SelectMode::extend);  // anchor readme.md -> gamma
    CHECK(column.selected_names() == std::vector<std::string>{"gamma", "readme.md"});

    fs::rename(f.root / "readme.md", f.root / "zz.md");
    CHECK(run_until(f.loop, [&] { return column.is_selected("zz.md"); }));
    CHECK(column.selected_count() == 2);

    fs::remove_all(f.root / "gamma");
    CHECK(run_until(f.loop, [&] { return !column.is_selected("gamma"); }));
    CHECK(column.selected_names() == std::vector<std::string>{"zz.md"});
}
