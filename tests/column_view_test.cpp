#include "nordend/views/column_view.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
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

    void press(xkb_keysym_t key) {
        nordend::ui::KeyEvent event;
        event.keysym = key;
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
