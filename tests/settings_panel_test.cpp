#include "nordend/views/settings_panel.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
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

using namespace std::chrono_literals;

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
             .file = "~/.config/nordend/config.toml",
             .families =
                 [] {
                     return std::vector<nordend::ui::FontFamily>{
                         {.name = "DejaVu Sans"},
                         {.name = "DejaVu Sans Mono", .monospace = true},
                         {.name = "Inter"},
                         {.name = "JetBrains Mono", .monospace = true}};
                 }});
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

namespace {

using nordend::ui::FontFamily;

std::vector<std::string> names(const std::vector<FontFamily>& families) {
    std::vector<std::string> out;
    out.reserve(families.size());
    for (const FontFamily& family : families) {
        out.push_back(family.name);
    }
    return out;
}

const std::vector<FontFamily> kFamilies{{.name = "DejaVu Sans"},
                                        {.name = "DejaVu Sans Mono", .monospace = true},
                                        {.name = "Inter"},
                                        {.name = "JetBrains Mono", .monospace = true},
                                        {.name = "Noto Sans Mono"}};

}  // namespace

TEST_CASE("font suggestions: what is typed, case aside, generic names and fixed width first") {
    using nordend::views::font_suggestions;
    CHECK(names(font_suggestions(kFamilies, "", false)) ==
          std::vector<std::string>{"sans-serif", "serif", "monospace", "DejaVu Sans",
                                   "DejaVu Sans Mono", "Inter", "JetBrains Mono",
                                   "Noto Sans Mono"});
    // Names starting with it before names holding it.
    CHECK(names(font_suggestions(kFamilies, "MONO", false)) ==
          std::vector<std::string>{"monospace", "DejaVu Sans Mono", "JetBrains Mono",
                                   "Noto Sans Mono"});
    CHECK(names(font_suggestions(kFamilies, "in", false)) ==
          std::vector<std::string>{"Inter", "JetBrains Mono"});
    // For the monospace font: the fixed-width families first.
    CHECK(names(font_suggestions(kFamilies, "", true)) ==
          std::vector<std::string>{"monospace", "sans-serif", "serif", "DejaVu Sans Mono",
                                   "JetBrains Mono", "DejaVu Sans", "Inter", "Noto Sans Mono"});
    // A pattern's properties do not filter.
    CHECK(names(font_suggestions(kFamilies, "inter:weight=medium", false)) ==
          std::vector<std::string>{"Inter"});
    CHECK(font_suggestions(kFamilies, "comic", false).empty());
}

TEST_CASE("font known: installed, generic, or a pattern") {
    using nordend::views::font_known;
    CHECK(font_known(kFamilies, "inter"));
    CHECK(font_known(kFamilies, " DejaVu Sans Mono "));
    CHECK(font_known(kFamilies, "sans-serif"));
    CHECK(font_known(kFamilies, "Comic Sans:weight=bold"));  // fontconfig's to resolve
    CHECK_FALSE(font_known(kFamilies, "Comic Sans"));
    CHECK_FALSE(font_known(kFamilies, "Inte"));
}

TEST_CASE("settings: the installed fonts offered while a font is changed") {
    Fixture f;
    f.press(key(XKB_KEY_comma, kCtrl));
    REQUIRE(run_until(f.loop, [&] { return f.panel().has_families(); }));  // read on a worker
    f.press(key(XKB_KEY_Return));
    CHECK(f.panel().suggestions().size() == 7);  // all: nothing typed yet
    f.type("mono");
    CHECK(names(f.panel().suggestions()) ==
          std::vector<std::string>{"monospace", "DejaVu Sans Mono", "JetBrains Mono"});
    CHECK_FALSE(f.panel().picked());
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_Down));
    CHECK(f.panel().picked() == 1);
    f.press(key(XKB_KEY_Up));
    f.press(key(XKB_KEY_Up));
    CHECK_FALSE(f.panel().picked());  // above the first: back to what is typed
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_Return));
    CHECK_FALSE(f.panel().editing());
    CHECK(f.writes.back() == "font.family = \"DejaVu Sans Mono\"");

    // A name not installed is refused, the list showing what is.
    f.press(key(XKB_KEY_Return));
    f.type("Comic Sans");
    f.press(key(XKB_KEY_Return));
    CHECK(f.panel().editing());
    CHECK(f.panel().problem() == "not installed: choose one from the list");
    CHECK(f.writes.back() == "font.family = \"DejaVu Sans Mono\"");
    // A pattern with properties is taken as written.
    f.press(key(XKB_KEY_a, kCtrl));
    f.type("Inter:weight=medium");
    f.press(key(XKB_KEY_Return));
    CHECK_FALSE(f.panel().editing());
    CHECK(f.writes.back() == "font.family = \"Inter:weight=medium\"");
    // Not offered for numbers.
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_Down));
    f.press(key(XKB_KEY_Return));
    CHECK(f.panel().suggestions().empty());
}
