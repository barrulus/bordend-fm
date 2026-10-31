#include "nordend/views/settings_panel.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <doctest/doctest.h>

#include "nordend/core/config.hpp"
#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/views/browser.hpp"
#include "temp_dir.hpp"

using nordend::core::EventLoop;
using nordend::test::TempDir;
using nordend::ui::KeyEvent;
using nordend::ui::Modifiers;
using nordend::views::Browser;
using nordend::views::SettingsPanel;

namespace {

KeyEvent key(xkb_keysym_t keysym, Modifiers modifiers = {}, std::string text = {}) {
    KeyEvent event;
    event.keysym = keysym;
    event.modifiers = modifiers;
    event.text = std::move(text);
    return event;
}

KeyEvent typed(std::string text) { return key(XKB_KEY_NoSymbol, {}, std::move(text)); }

const Modifiers kCtrl{.ctrl = true};

// A browser whose settings live in a config.toml held in memory, as main's write does: edited
// as text, checked by parsing, kept only when good.
struct Fixture {
    TempDir dir;
    EventLoop loop;
    nordend::core::WorkerPool pool{loop, 2};
    nordend::fs::DirectoryWatcher watcher{loop};
    Browser browser{loop,    pool,
                    watcher, nordend::fs::MimeDatabase::load(),
                    [] {},   [](nordend::ui::CursorShape /*shape*/) {}};
    std::string file = "# mine\n[pointer]\ndouble_click_ms = 400 # slow\n";
    std::vector<std::string> writes;  // "section.key = value", the ones asked for

    Fixture() {
        browser.set_home_trash(dir.path / "Trash");
        browser.set_settings(
            {.read = [this] { return nordend::core::parse_config(file, "config.toml").config; },
             .write =
                 [this](std::string_view section, std::string_view key, const std::string& value) {
                     writes.push_back(std::string{section} + "." + std::string{key} + " = " +
                                      value);
                     std::string edited =
                         nordend::core::set_config_value(file, section, key, value);
                     std::string problem = nordend::core::config_problem(
                         nordend::core::parse_config(edited, "config.toml"), section, key);
                     if (problem.empty()) {
                         file = std::move(edited);
                     }
                     return problem;
                 },
             .colours = [] { return std::string{"Nordend's own"}; },
             .fonts_in_use = [] { return nordend::core::Config::Font{}; },
             .file = "~/.config/nordend/config.toml"});
        browser.open(dir.path);
    }

    bool press(const KeyEvent& event) { return browser.key(event, Browser::Clock::now()); }

    void type(std::string_view text) {
        for (const char c : text) {
            press(typed(std::string{c}));
        }
    }

    [[nodiscard]] const SettingsPanel& panel() { return browser.settings_panel(); }
};

}  // namespace

TEST_CASE("settings: Ctrl+, opens and closes the panel, which takes every key meanwhile") {
    Fixture f;
    CHECK(f.press(key(XKB_KEY_comma, kCtrl)));
    CHECK(f.panel().is_open());
    CHECK(f.press(key(XKB_KEY_2, kCtrl)));  // the grid's shortcut: the panel's now
    CHECK(f.browser.mode() == Browser::Mode::columns);
    f.press(key(XKB_KEY_comma, kCtrl));
    CHECK_FALSE(f.panel().is_open());

    f.press(key(XKB_KEY_comma, kCtrl));
    f.press(key(XKB_KEY_Escape));
    CHECK_FALSE(f.panel().is_open());
    // Listed with the other shortcuts.
    CHECK(nordend::views::keys_for(nordend::views::Command::settings) == "Ctrl+,");
}

TEST_CASE("settings: choose a value, change it, and the file says so with its comments") {
    Fixture f;
    f.press(key(XKB_KEY_comma, kCtrl));
    CHECK(f.panel().chosen() == 0);
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_Tab));
    CHECK(f.panel().chosen() == 2);  // the double-click time
    f.press(key(XKB_KEY_Up));
    f.press(key(XKB_KEY_ISO_Left_Tab, {.shift = true}));
    f.press(key(XKB_KEY_Up));
    CHECK(f.panel().chosen() == 3);  // around the end

    f.press(key(XKB_KEY_Up));
    f.press(key(XKB_KEY_Return));
    REQUIRE(f.panel().editing());
    CHECK(f.panel().field().text() == "400");
    f.type("650");  // the value was selected: typing replaces it
    f.press(key(XKB_KEY_Return));
    CHECK_FALSE(f.panel().editing());
    CHECK(f.writes == std::vector<std::string>{"pointer.double_click_ms = 650"});
    CHECK(f.file == "# mine\n[pointer]\ndouble_click_ms = 650 # slow\n");

    // A font: written as a TOML string, in a section the file did not have.
    f.press(key(XKB_KEY_Up));
    f.press(key(XKB_KEY_Up));
    f.press(key(XKB_KEY_Return));
    CHECK(f.panel().field().text() == "sans-serif");
    f.type("Inter \"Display\"");
    f.press(key(XKB_KEY_Return));
    CHECK(nordend::core::parse_config(f.file, "config.toml").config.font.family ==
          "Inter \"Display\"");
    CHECK(f.file.starts_with("# mine\n[pointer]\ndouble_click_ms = 650 # slow\n"));
}

TEST_CASE("settings: a value refused stays to be fixed; Escape drops it") {
    Fixture f;
    f.press(key(XKB_KEY_comma, kCtrl));
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_Return));
    f.type("5");
    f.press(key(XKB_KEY_Return));
    CHECK(f.panel().editing());
    CHECK(f.panel().problem() == "must be between 100 and 2000, got 5");
    CHECK(f.file.find("= 400") != std::string::npos);  // nothing written

    // Not a number at all: refused before anything is asked.
    f.press(key(XKB_KEY_a, kCtrl));
    f.type("fast");
    const std::size_t writes = f.writes.size();
    f.press(key(XKB_KEY_Return));
    CHECK(f.panel().problem() == "a whole number, please");
    CHECK(f.writes.size() == writes);

    f.press(key(XKB_KEY_Escape));  // drops the change, the panel stays
    CHECK_FALSE(f.panel().editing());
    CHECK(f.panel().problem().empty());
    CHECK(f.panel().is_open());
    f.press(key(XKB_KEY_Escape));
    CHECK_FALSE(f.panel().is_open());
}

TEST_CASE("settings: pasted text goes into the value being changed, its first line") {
    Fixture f;
    f.press(key(XKB_KEY_comma, kCtrl));
    f.press(key(XKB_KEY_Return));
    f.browser.settings_panel().paste("IBM Plex Sans\nsecond line");
    CHECK(f.panel().field().text() == "IBM Plex Sans");
}
