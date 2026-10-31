#include "nordend/views/state.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/file.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/views/browser.hpp"
#include "temp_dir.hpp"

using nordend::core::EventLoop;
using nordend::fs::SortKey;
using nordend::test::TempDir;
using nordend::views::Browser;
using nordend::views::format_state;
using nordend::views::load_state;
using nordend::views::parse_state;
using nordend::views::State;
using nordend::views::StateKeeper;
using nordend::views::StateResult;
using namespace std::chrono_literals;

namespace {

StateResult parse(std::string_view text) { return parse_state(text, "state.toml"); }

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

// Every value away from its default.
State changed() {
    State s;
    s.mode = State::Mode::details;
    s.show_hidden = true;
    s.sidebar_visible = false;
    s.sidebar_width = 260.5F;
    s.info_visible = false;
    s.info_width = 410.0F;
    s.info_expanded = true;
    s.sort_key = SortKey::modified;
    s.sort_descending = true;
    s.details_widths = {80.0F, 170.0F, 240.0F};
    s.window_width = 1400;
    s.window_height = 900;
    s.maximized = true;
    return s;
}

}  // namespace

TEST_CASE("state: written and read back the same") {
    const StateResult result = parse(format_state(changed()));
    CHECK(result.problems.empty());
    CHECK(result.state == changed());

    const StateResult defaults = parse(format_state(State{}));
    CHECK(defaults.problems.empty());
    CHECK(defaults.state == State{});
}

TEST_CASE("state: empty or missing is the defaults, quietly") {
    CHECK(parse("").state == State{});
    CHECK(parse("").problems.empty());
    const TempDir dir;
    const StateResult missing = load_state(dir.path / "none" / "state.toml");
    CHECK(missing.state == State{});
    CHECK(missing.problems.empty());
}

TEST_CASE("state: bad values keep their default, each reported where it is") {
    const StateResult result = parse(R"([view]
mode = "tree"
show_hidden = "yes"

[sidebar]
width = 5

[window]
width = 100000
height = 700
)");
    CHECK(result.state.mode == State::Mode::columns);
    CHECK_FALSE(result.state.show_hidden);
    CHECK(result.state.sidebar_width == State{}.sidebar_width);
    CHECK(result.state.window_width == State{}.window_width);
    CHECK(result.state.window_height == 700);  // the good one is taken
    REQUIRE(result.problems.size() == 4);
    CHECK(result.problems[0].starts_with("state.toml:2:"));
    CHECK(result.problems[0].find("view.mode") != std::string::npos);
    CHECK(result.problems[1].starts_with("state.toml:3:"));
    CHECK(result.problems[2].starts_with("state.toml:6:"));
    CHECK(result.problems[3].starts_with("state.toml:9:"));
}

TEST_CASE("state: a broken file is the defaults; unknown keys are fine") {
    const StateResult broken = parse("[view\nmode = \"grid\"\n");
    CHECK(broken.state == State{});
    REQUIRE(broken.problems.size() == 1);
    CHECK(broken.problems[0].starts_with("state.toml:1:"));

    const StateResult newer = parse("[view]\nmode = \"grid\"\nzoom = 3\n[tabs]\nopen = 2\n");
    CHECK(newer.problems.empty());
    CHECK(newer.state.mode == State::Mode::grid);
}

TEST_CASE("state: the file lives under XDG_STATE_HOME") {
    const char* home = std::getenv("XDG_STATE_HOME");  // NOLINT(concurrency-mt-unsafe)
    REQUIRE(home != nullptr);  // tests/main.cpp points it into the test root
    CHECK(nordend::views::default_state_path() ==
          std::filesystem::path{home} / "nordend" / "state.toml");
}

TEST_CASE("state: the browser gives back what it was given") {
    const TempDir dir;
    EventLoop loop;
    nordend::core::WorkerPool pool{loop, 2};
    nordend::fs::DirectoryWatcher watcher{loop};
    Browser browser{loop,    pool,
                    watcher, nordend::fs::MimeDatabase::load(),
                    [] {},   [](nordend::ui::CursorShape /*shape*/) {}};
    CHECK(browser.state() == State{});  // the browser's defaults are the file's

    State wanted = changed();
    browser.apply(wanted);
    browser.open(dir.path);
    REQUIRE(run_until(loop, [&] { return !browser.loading(); }));
    CHECK(browser.mode() == Browser::Mode::details);
    CHECK(browser.shows_hidden());
    CHECK_FALSE(browser.sidebar_visible());
    CHECK(browser.details().folder()->order().key == SortKey::modified);
    // The window's part is main's: the browser leaves it at its defaults.
    wanted.window_width = State{}.window_width;
    wanted.window_height = State{}.window_height;
    wanted.maximized = false;
    CHECK(browser.state() == wanted);

    // Narrower than a column's minimum: the view keeps its minimum.
    State narrow = wanted;
    narrow.details_widths = {10.0F, 10.0F, 10.0F};
    browser.apply(narrow);
    for (const float width : browser.state().details_widths) {
        CHECK(width >= 64.0F);
    }
    // After open(), a mode applied is as if chosen.
    State grid = wanted;
    grid.mode = State::Mode::grid;
    browser.apply(grid);
    REQUIRE(run_until(loop, [&] { return !browser.loading(); }));
    CHECK(browser.mode() == Browser::Mode::grid);
}

TEST_CASE("StateKeeper: one write after the last change, flush writes what is left") {
    const TempDir dir;
    const std::filesystem::path file = dir.path / "nordend" / "state.toml";
    EventLoop loop;
    nordend::core::WorkerPool pool{loop, 2};
    StateKeeper keeper{loop, pool, file, State{}, 20ms};

    keeper.note(State{});  // nothing changed: nothing to write
    State first;
    first.mode = State::Mode::grid;
    State second = first;
    second.show_hidden = true;
    keeper.note(first);
    keeper.note(second);  // within the delay: only this one is written
    REQUIRE(run_until(loop, [&] { return keeper.saved() == second; }));
    REQUIRE(run_until(loop, [&] { return load_state(file).state == second; }));

    State third = second;
    third.sidebar_width = 300.0F;
    keeper.note(third);
    keeper.flush();  // at exit: written now, not after the delay
    CHECK(load_state(file).state == third);
    CHECK(load_state(file).problems.empty());
}
