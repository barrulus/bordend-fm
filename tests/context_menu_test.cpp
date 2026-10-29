#include "nordend/views/context_menu.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <doctest/doctest.h>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/fs/operations.hpp"
#include "nordend/fs/places.hpp"
#include "nordend/views/browser.hpp"
#include "temp_dir.hpp"

using nordend::core::EventLoop;
using nordend::test::TempDir;
using nordend::ui::KeyEvent;
using nordend::ui::Modifiers;
using nordend::views::Browser;
using nordend::views::ContextMenu;
using nordend::views::Folder;
using nordend::views::MenuItem;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

using Names = std::vector<std::string>;

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

std::string read_file(const fs::path& file) {
    std::ifstream in{file};
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

MenuItem item(std::string id, bool enabled = true, std::function<void()> action = nullptr) {
    MenuItem out;
    out.label = id;
    out.id = std::move(id);
    out.enabled = enabled;
    out.action = std::move(action);
    return out;
}

// The ids of the menu's items, and which can be chosen ("-paste": shown, not usable).
Names ids(const std::vector<MenuItem>& items) {
    Names out;
    for (const MenuItem& entry : items) {
        out.push_back(entry.enabled ? entry.id : "-" + entry.id);
    }
    return out;
}

// photos/{a.txt, b.txt, P1.ORF, P1.ORF.xmp, sub/}, shown in the details.
struct Fixture {
    TempDir dir;
    fs::path photos = dir.path / "photos";
    EventLoop loop;
    nordend::core::WorkerPool pool{loop, 2};
    nordend::fs::DirectoryWatcher watcher{loop};
    Browser browser{loop,    pool,
                    watcher, nordend::fs::MimeDatabase::load(),
                    [] {},   [](nordend::ui::CursorShape /*shape*/) {}};
    // What "Open With" was asked and did.
    std::vector<Browser::AppChoice> apps{
        {.id = "viewer.desktop", .name = "Viewer", .is_default = true},
        {.id = "editor.desktop", .name = "Editor"}};
    std::string asked_type;
    std::string opened_with;
    Names opened;

    Fixture() {
        fs::create_directories(photos / "sub");
        for (const char* name : {"a.txt", "b.txt", "P1.ORF", "P1.ORF.xmp"}) {
            std::ofstream{photos / name} << name;
        }
        browser.set_home_trash(dir.path / "Trash");
        browser.set_templates_dir(dir.path / "templates");
        browser.set_open_with_handlers(
            [this](const fs::path& /*directory*/, const nordend::fs::Entry& entry,
                   const std::function<void(std::vector<Browser::AppChoice>)>& done) {
                asked_type = entry.name;
                done(apps);
            },
            [this](const fs::path& /*directory*/, const std::vector<nordend::fs::Entry>& files,
                   std::string app_id) {
                opened_with = std::move(app_id);
                opened.clear();
                for (const nordend::fs::Entry& file : files) {
                    opened.push_back(file.name);
                }
            });
        browser.open(photos);
        browser.set_mode(Browser::Mode::details);
        REQUIRE(settle());
    }

    bool settle() {
        return run_until(loop, [this] { return !browser.loading(); });
    }

    bool press(const KeyEvent& event) { return browser.key(event, Browser::Clock::now()); }

    std::shared_ptr<Folder> folder() { return browser.details().folder(); }

    ContextMenu& menu() { return browser.context_menu(); }

    void select(const Names& names) {
        folder()->set_cursor(std::nullopt);
        for (const std::string& name : names) {
            const auto& entries = folder()->entries();
            const auto it = std::ranges::find(entries, name, &nordend::fs::Entry::name);
            REQUIRE(it != entries.end());
            folder()->set_cursor(static_cast<std::size_t>(it - entries.begin()),
                                 nordend::views::SelectMode::toggle);
        }
    }

    // Down until the item called id is highlighted, then Enter.
    void choose(const std::string& id) {
        for (std::size_t i = 0; i < 20; ++i) {
            const std::optional<std::size_t> at = menu().highlighted();
            if (at && menu().items()[*at].id == id) {
                CHECK(press(key(XKB_KEY_Return)));
                return;
            }
            press(key(XKB_KEY_Down));
        }
        FAIL("no item " << id);
    }
};

}  // namespace

TEST_CASE("context menu: the keyboard moves over what can be chosen, in and out of a submenu") {
    ContextMenu menu{[] {}};
    int ran = 0;
    MenuItem with = item("with");
    with.submenu = true;
    with.children = {item("first", true, [&ran] { ran = 1; }), item("off", false),
                     item("last", true, [&ran] { ran = 3; })};
    MenuItem later = item("later");
    later.submenu = true;
    later.empty_text = "Looking…";
    menu.open(10, 10, {item("open", true, [&ran] { ran = 10; }), item("paste", false), with, later},
              true);
    REQUIRE(menu.is_open());
    CHECK(menu.highlighted() == 0);  // from the keyboard: the first, at once
    CHECK(menu.key(key(XKB_KEY_Down)));
    CHECK(menu.highlighted() == 2);  // over Paste, which cannot be chosen
    CHECK(menu.key(key(XKB_KEY_Up)));
    CHECK(menu.highlighted() == 0);
    CHECK(menu.key(key(XKB_KEY_Up)));
    CHECK(menu.highlighted() == 3);  // wraps
    CHECK(menu.key(key(XKB_KEY_Home)));
    CHECK(menu.highlighted() == 0);
    CHECK(menu.key(key(XKB_KEY_q)));  // every key is the menu's: nothing happens
    CHECK(menu.is_open());

    // Right opens the submenu, its first item highlighted; Down skips the disabled one.
    CHECK(menu.key(key(XKB_KEY_Down)));
    CHECK(menu.key(key(XKB_KEY_Right)));
    CHECK(menu.submenu_open());
    CHECK(menu.sub_highlighted() == 0);
    CHECK(menu.key(key(XKB_KEY_Down)));
    CHECK(menu.sub_highlighted() == 2);
    CHECK(menu.key(key(XKB_KEY_Left)));  // back out
    CHECK_FALSE(menu.submenu_open());
    CHECK(menu.highlighted() == 2);
    CHECK(menu.key(key(XKB_KEY_Return)));  // Enter on a submenu opens it
    CHECK(menu.submenu_open());
    CHECK(menu.key(key(XKB_KEY_Escape)));  // Escape: out of the submenu first
    CHECK(menu.is_open());
    CHECK_FALSE(menu.submenu_open());

    // A submenu filled later: the keyboard waiting in it lands on its first item.
    CHECK(menu.key(key(XKB_KEY_Down)));
    CHECK(menu.key(key(XKB_KEY_Right)));
    CHECK(menu.submenu_open());
    CHECK_FALSE(menu.sub_highlighted().has_value());
    menu.set_submenu("later", {item("found", true, [&ran] { ran = 4; })});
    CHECK(menu.sub_highlighted() == 0);
    CHECK(menu.key(key(XKB_KEY_Return)));
    CHECK(ran == 4);
    CHECK_FALSE(menu.is_open());  // closed before the action ran

    // Escape closes; Enter runs an item.
    menu.open(10, 10, {item("open", true, [&ran] { ran = 10; })}, true);
    CHECK(menu.key(key(XKB_KEY_Escape)));
    CHECK_FALSE(menu.is_open());
    menu.open(10, 10, {item("open", true, [&ran] { ran = 10; })}, true);
    CHECK(menu.key(key(XKB_KEY_Return)));
    CHECK(ran == 10);

    // A click before it was ever drawn lands outside: it closes, nothing runs.
    ran = 0;
    menu.open(10, 10, {item("open", true, [&ran] { ran = 10; })}, false);
    CHECK_FALSE(menu.highlighted().has_value());  // by the pointer: nothing highlighted yet
    nordend::ui::PointerEvent click;
    click.kind = nordend::ui::PointerEvent::Kind::press;
    click.button = nordend::ui::PointerButton::left;
    CHECK(menu.pointer(click));
    CHECK_FALSE(menu.is_open());
    CHECK(ran == 0);
}

TEST_CASE("context menu: the items for one entry, several, empty space; paste once taken") {
    Fixture f;
    // One file: everything, Rename too; nothing taken yet, so no Paste.
    f.select({"a.txt"});
    CHECK(f.press(key(XKB_KEY_Menu)));
    REQUIRE(f.menu().is_open());
    CHECK(ids(f.menu().items()) ==
          Names{"open", "open_with", "copy", "cut", "-paste", "rename", "trash"});
    CHECK(f.menu().items()[2].keys == "Ctrl+C");  // from the shortcuts table
    CHECK(f.menu().items()[6].keys == "Delete");
    CHECK(f.press(key(XKB_KEY_Escape)));
    CHECK_FALSE(f.menu().is_open());

    // Several: no Rename. A folder: no Open With.
    f.select({"a.txt", "b.txt"});
    f.browser.open_menu(100, 100, false);
    CHECK(ids(f.menu().items()) ==
          Names{"open", "open_with", "copy", "cut", "-paste", "-rename", "trash"});
    f.menu().close();
    f.select({"sub"});
    CHECK(f.press(key(XKB_KEY_F10, {.shift = true})));
    CHECK(ids(f.menu().items()) == Names{"open", "copy", "cut", "-paste", "rename", "trash"});

    // Copy from the menu; then Paste can be chosen, here and on empty space.
    f.choose("copy");
    CHECK_FALSE(f.menu().is_open());
    CHECK(f.browser.status_bar().message() == "Ctrl+V copies “sub” where you want them");
    f.select({});
    CHECK(f.press(key(XKB_KEY_Menu)));
    CHECK(ids(f.menu().items()) == Names{"new_folder", "new_file", "paste"});
    REQUIRE(f.settle());  // the templates looked for: none
    CHECK(ids(f.menu().items()[1].children) == Names{"empty_file"});
}

TEST_CASE("context menu: Open With lists the applications and opens the selection with one") {
    Fixture f;
    f.select({"sub", "b.txt", "a.txt"});  // the cursor on a.txt
    REQUIRE(f.folder()->cursor_entry() != nullptr);
    const std::string cursor = f.folder()->cursor_entry()->name;
    REQUIRE(cursor == "a.txt");
    CHECK(f.press(key(XKB_KEY_Menu)));
    CHECK(f.asked_type == cursor);  // the applications of the cursor's file
    const MenuItem& with = f.menu().items()[1];
    REQUIRE(with.id == "open_with");
    REQUIRE(with.children.size() == 2);
    CHECK(with.children[0].label == "Viewer (default)");
    CHECK(with.children[1].label == "Editor");
    CHECK(f.press(key(XKB_KEY_Down)));
    CHECK(f.press(key(XKB_KEY_Right)));
    CHECK(f.press(key(XKB_KEY_Down)));
    CHECK(f.press(key(XKB_KEY_Return)));
    CHECK(f.opened_with == "editor.desktop");
    std::ranges::sort(f.opened);
    CHECK(f.opened == Names{"a.txt", "b.txt"});  // the files; folders are not opened

    // No applications: a dimmed line says so.
    f.apps.clear();
    CHECK(f.press(key(XKB_KEY_Menu)));
    REQUIRE(f.menu().items()[1].children.size() == 1);
    CHECK(f.menu().items()[1].children[0].label == "No applications");
    CHECK_FALSE(f.menu().items()[1].children[0].enabled);
}

TEST_CASE("context menu: Move to Trash and Rename from the menu do what their keys do") {
    Fixture f;
    f.select({"P1.ORF"});
    CHECK(f.press(key(XKB_KEY_Menu)));
    f.choose("trash");
    REQUIRE(f.settle());
    CHECK(fs::exists(f.dir.path / "Trash" / "files" / "P1.ORF.xmp"));
    CHECK_FALSE(fs::exists(f.photos / "P1.ORF"));

    REQUIRE(run_until(f.loop, [&] { return f.folder()->cursor_entry() != nullptr; }));
    CHECK(f.press(key(XKB_KEY_Menu)));
    f.choose("rename");
    CHECK(f.browser.name_editor().active());
}

TEST_CASE("context menu: New File, empty or from a template, never over an existing file") {
    Fixture f;
    const fs::path templates = f.dir.path / "templates";
    fs::create_directories(templates / "folder");
    std::ofstream{templates / "Letter.txt"} << "Dear,";
    chmod((templates / "Letter.txt").c_str(), 0640);
    std::ofstream{templates / ".hidden"} << "x";
    std::ofstream{templates / "Notes.md~"} << "x";
    CHECK(nordend::fs::templates_in(templates) == Names{"Letter.txt"});

    f.select({});
    CHECK(f.press(key(XKB_KEY_Menu)));
    REQUIRE(f.settle());
    CHECK(ids(f.menu().items()[1].children) == Names{"empty_file", "template:Letter.txt"});
    CHECK(f.press(key(XKB_KEY_Down)));
    CHECK(f.press(key(XKB_KEY_Right)));
    CHECK(f.press(key(XKB_KEY_Return)));  // Empty File
    REQUIRE(f.settle());
    CHECK(fs::is_regular_file(f.photos / "New File"));
    CHECK(fs::file_size(f.photos / "New File") == 0);
    REQUIRE(run_until(f.loop, [&] { return f.browser.name_editor().active(); }));
    f.browser.name_editor().cancel();

    for (int i = 0; i < 2; ++i) {
        f.select({});
        CHECK(f.press(key(XKB_KEY_Menu)));
        REQUIRE(f.settle());
        CHECK(f.press(key(XKB_KEY_Down)));
        CHECK(f.press(key(XKB_KEY_Right)));
        CHECK(f.press(key(XKB_KEY_Down)));
        CHECK(f.press(key(XKB_KEY_Return)));  // Letter.txt
        REQUIRE(f.settle());
        f.browser.name_editor().cancel();
    }
    CHECK(read_file(f.photos / "Letter.txt") == "Dear,");
    CHECK(read_file(f.photos / "Letter 2.txt") == "Dear,");
    CHECK((fs::status(f.photos / "Letter 2.txt").permissions() & fs::perms::all) ==
          (fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read));
}

TEST_CASE("new file: numbered names, a template's contents, nothing left on failure") {
    const TempDir dir;
    std::ofstream{dir.path / "New File"} << "old";
    const nordend::fs::MadeFile empty = nordend::fs::make_file(dir.path, "New File");
    CHECK(empty.name == "New File 2");
    CHECK(read_file(dir.path / "New File") == "old");
    CHECK(nordend::fs::make_file(dir.path, "a.tar.gz").name == "a.tar.gz");
    CHECK(nordend::fs::make_file(dir.path, "a.tar.gz").name == "a.tar 2.gz");
    const nordend::fs::MadeFile missing =
        nordend::fs::make_file(dir.path, "x.txt", dir.path / "no template");
    CHECK(missing.error == std::errc::no_such_file_or_directory);
    CHECK_FALSE(fs::exists(dir.path / "x.txt"));
}

TEST_CASE("new file: the templates folder, as the user dirs say") {
    const TempDir dir;
    nordend::fs::PlaceSources sources{.home = dir.path / "home",
                                      .config_home = dir.path / "config"};
    fs::create_directories(sources.home / "Templates");
    fs::create_directories(sources.home / "Modelli");
    fs::create_directories(sources.config_home);
    CHECK(nordend::fs::templates_dir(sources) == sources.home / "Templates");  // no user-dirs
    std::ofstream{sources.config_home / "user-dirs.dirs"}
        << "XDG_PICTURES_DIR=\"$HOME/Pictures\"\n";
    CHECK(nordend::fs::templates_dir(sources).empty());  // the file says nothing about it
    std::ofstream{sources.config_home / "user-dirs.dirs"}
        << "XDG_TEMPLATES_DIR=\"$HOME/Modelli\"\n";
    CHECK(nordend::fs::templates_dir(sources) == sources.home / "Modelli");
    std::ofstream{sources.config_home / "user-dirs.dirs"} << "XDG_TEMPLATES_DIR=\"$HOME/Nope\"\n";
    CHECK(nordend::fs::templates_dir(sources).empty());  // not there
}
