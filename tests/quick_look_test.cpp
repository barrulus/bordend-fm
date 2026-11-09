#include "nordend/views/quick_look.hpp"

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
#include <utility>
#include <variant>
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
using nordend::core::WorkerPool;
using nordend::views::Browser;
using nordend::views::QuickLook;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

const fs::path kData = fs::path{NORDEND_TEST_DATA} / "image";

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

// dir/{sub/, a.png (48x32), b.jpg (EXIF orientation 6: 32x48 upright), notes.txt}
struct Fixture {
    nordend::test::TempDir dir;
    EventLoop loop;
    WorkerPool pool{loop, 2};
    nordend::fs::DirectoryWatcher watcher{loop};
    Browser browser{loop,    pool,
                    watcher, nordend::fs::MimeDatabase::load(),
                    [] {},   [](nordend::ui::CursorShape /*shape*/) {}};

    explicit Fixture(bool sniff = true) {
        fs::create_directory(dir.path / "sub");
        fs::copy_file(kData / "blocks.png", dir.path / "a.png");
        fs::copy_file(kData / "blocks-orient6.jpg", dir.path / "b.jpg");
        std::ofstream{dir.path / "notes.txt"} << "notes";
        browser.open(dir.path);
        // Nothing is drawn here: ask for the types the views would sniff for their rows.
        run_until(loop, [this] { return !browser.loading(); });
        if (sniff) {
            browser.columns().column(0).folder().sniff(0, 10);
        }
    }

    // Loaded, and every entry's type known (a name like a.png may need its content).
    bool settle() {
        return run_until(loop, [this] {
            const auto& folder = browser.columns().column(0).folder();
            for (const nordend::fs::Entry& entry : folder.entries()) {
                if (folder.mime_of(entry) == nordend::fs::kUnknownMime) {
                    return false;
                }
            }
            return !browser.loading();
        });
    }

    void press(xkb_keysym_t key, nordend::ui::Modifiers modifiers = {}) {
        nordend::ui::KeyEvent event;
        event.keysym = key;
        event.modifiers = modifiers;
        browser.key(event, Browser::Clock::now());
    }

    using Size = std::optional<std::pair<std::uint32_t, std::uint32_t>>;

    Size image_size() { return browser.quick_look().image_size(); }
};

}  // namespace

TEST_CASE("quick look: Space opens it on the cursor, arrows walk, Space and Escape close") {
    Fixture f;
    REQUIRE(f.settle());
    auto& look = f.browser.quick_look();
    f.press(XKB_KEY_space);  // no cursor yet: nothing to show
    CHECK_FALSE(look.is_open());

    f.press(XKB_KEY_Down);  // sub
    f.press(XKB_KEY_Down);  // a.png
    f.press(XKB_KEY_space);
    REQUIRE(look.is_open());
    CHECK(look.shown() == "a.png");
    REQUIRE(f.settle());
    CHECK(f.image_size() == Fixture::Size{{48, 32}});

    f.press(XKB_KEY_Down);  // the view moves, Quick Look follows
    CHECK(look.shown() == "b.jpg");
    REQUIRE(f.settle());
    CHECK(f.image_size() == Fixture::Size{{32, 48}});  // upright

    f.press(XKB_KEY_Down);  // text: a page of lines
    CHECK(look.shown() == "notes.txt");
    REQUIRE(f.settle());
    CHECK_FALSE(f.image_size());
    REQUIRE(look.content() != nullptr);
    const auto* text = std::get_if<nordend::preview::Text>(look.content());
    REQUIRE(text != nullptr);
    CHECK(text->lines == std::vector<std::string>{"notes"});

    f.press(XKB_KEY_Up);
    f.press(XKB_KEY_Escape);
    CHECK_FALSE(look.is_open());
    CHECK(f.browser.columns().column(0).folder().cursor_entry()->name == "b.jpg");

    f.press(XKB_KEY_space);
    CHECK(look.is_open());
    f.press(XKB_KEY_space);
    CHECK_FALSE(look.is_open());
}

TEST_CASE("quick look: Escape closes it without reaching the view's selection") {
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_Down);
    f.press(XKB_KEY_Down);                   // a.png
    f.press(XKB_KEY_Down, {.shift = true});  // a.png, b.jpg
    auto& folder = f.browser.columns().column(0).folder();
    REQUIRE(folder.selected_count() == 2);

    f.press(XKB_KEY_space);
    CHECK(f.browser.quick_look().shown() == "b.jpg");
    f.press(XKB_KEY_Escape);
    CHECK_FALSE(f.browser.quick_look().is_open());
    CHECK(folder.selected_count() == 2);  // a second Escape would collapse it

    f.press(XKB_KEY_space, {.ctrl = true});  // Ctrl+Space is selection, not Quick Look
    CHECK_FALSE(f.browser.quick_look().is_open());
    CHECK(folder.selected_count() == 1);
}

TEST_CASE("quick look: works over the grid, and a click closes it") {
    Fixture f;
    REQUIRE(f.settle());
    f.press(XKB_KEY_2, {.ctrl = true});
    f.press(XKB_KEY_Right);  // sub
    f.press(XKB_KEY_Right);  // a.png
    f.press(XKB_KEY_space);
    CHECK(f.browser.quick_look().shown() == "a.png");
    f.press(XKB_KEY_Right);
    CHECK(f.browser.quick_look().shown() == "b.jpg");
    nordend::ui::PointerEvent click;
    click.kind = nordend::ui::PointerEvent::Kind::press;
    click.button = nordend::ui::PointerButton::left;
    f.browser.pointer(click, Browser::Clock::now());
    CHECK_FALSE(f.browser.quick_look().is_open());
}

TEST_CASE("quick look: an image whose type is still unknown is shown once it is sniffed") {
    Fixture f{false};
    f.press(XKB_KEY_Down);
    f.press(XKB_KEY_Down);  // a.png: its name alone does not decide its type
    auto& folder = f.browser.columns().column(0).folder();
    REQUIRE(folder.mime_of(*folder.cursor_entry()) == nordend::fs::kUnknownMime);
    f.press(XKB_KEY_space);
    REQUIRE(f.browser.quick_look().is_open());
    REQUIRE(f.settle());  // Quick Look asked for its type; meanwhile, the information card
    CHECK_FALSE(f.image_size());
    f.browser.quick_look().follow(f.browser.columns().focused_folder());  // as every frame does
    REQUIRE(f.settle());
    CHECK(f.image_size() == Fixture::Size{{48, 32}});
}

TEST_CASE("quick look: a folder's card counts its entries; a wheel turn over text is taken") {
    Fixture f;
    REQUIRE(f.settle());
    auto& look = f.browser.quick_look();
    f.press(XKB_KEY_Down);  // sub
    f.press(XKB_KEY_space);
    REQUIRE(f.settle());
    REQUIRE(look.content() != nullptr);
    const auto* info = std::get_if<nordend::preview::Info>(look.content());
    REQUIRE(info != nullptr);
    CHECK(info->items == std::optional<std::uint64_t>{0});
    CHECK(info->permissions.front() == 'd');

    f.press(XKB_KEY_End);  // notes.txt
    REQUIRE(f.settle());
    nordend::ui::PointerEvent wheel;
    wheel.kind = nordend::ui::PointerEvent::Kind::scroll;
    wheel.wheel_y = 3.0F;
    f.browser.pointer(wheel, Browser::Clock::now());
    CHECK(look.is_open());  // scrolling does not close it
    CHECK(look.shown() == "notes.txt");
}

TEST_CASE("quick look: Ctrl+A and Ctrl+C take a text page's text, not the files") {
    Fixture f;
    std::ofstream{f.dir.path / "notes.txt"} << "first line\nsecond line\n";
    std::vector<nordend::views::SystemClipboard::Format> offered;
    nordend::views::SystemClipboard clipboard;
    clipboard.offer = [&](std::vector<nordend::views::SystemClipboard::Format> formats) {
        offered = std::move(formats);
    };
    f.browser.set_system_clipboard(std::move(clipboard));
    REQUIRE(f.settle());
    auto& look = f.browser.quick_look();
    f.press(XKB_KEY_End);  // notes.txt
    f.press(XKB_KEY_space);
    REQUIRE(f.settle());
    REQUIRE(look.shows_text());
    // Nothing selected: Ctrl+C copies the file, as without Quick Look.
    f.press(XKB_KEY_c, {.ctrl = true});
    REQUIRE_FALSE(offered.empty());
    CHECK(offered.front().first == "x-special/gnome-copied-files");
    f.press(XKB_KEY_a, {.ctrl = true});
    CHECK(look.selected_text() == "first line\nsecond line");
    f.press(XKB_KEY_c, {.ctrl = true});
    REQUIRE(offered.size() == 3);
    CHECK(offered.front().first == "text/plain;charset=utf-8");
    CHECK(offered.front().second == "first line\nsecond line");
    CHECK(look.is_open());
    // Walking to another entry drops the selection.
    f.press(XKB_KEY_Up);
    CHECK(look.selected_text().empty());
}

TEST_CASE("quick look: on a text page the pointer selects; a click beside it closes") {
    constexpr std::int32_t width = 1000;
    constexpr std::int32_t height = 700;
    std::unique_ptr<nordend::ui::Offscreen> gl;
    try {
        gl = std::make_unique<nordend::ui::Offscreen>(width, height);
    } catch (const std::runtime_error& error) {
        MESSAGE("no offscreen OpenGL: " << error.what());
        return;
    }
    nordend::ui::FontSystem fonts;
    nordend::ui::Renderer renderer{fonts};
    nordend::ui::IconSet icons{fonts};
    Fixture f;
    // Ten lines of the same words: the page's middle is on one of them.
    std::ofstream notes{f.dir.path / "notes.txt"};
    for (int i = 0; i < 10; ++i) {
        notes << "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu nu xi\n";
    }
    notes.close();
    REQUIRE(f.settle());
    auto& look = f.browser.quick_look();
    f.press(XKB_KEY_End);
    f.press(XKB_KEY_space);
    REQUIRE(f.settle());
    REQUIRE(look.shows_text());
    const auto draw = [&] {
        renderer.begin_frame(width, height, 1.0F, {});
        f.browser.draw(renderer, {0.0F, 0.0F, width, height}, nordend::theme::Palette{}, icons,
                       Browser::Clock::now());
        renderer.end_frame();
    };
    draw();
    const auto pointer = [&](nordend::ui::PointerEvent::Kind kind, float x, float y, int clicks) {
        nordend::ui::PointerEvent event;
        event.kind = kind;
        event.x = x;
        event.y = y;
        event.button = nordend::ui::PointerButton::left;
        event.click_count = clicks;
        f.browser.pointer(event, Browser::Clock::now());
    };
    using Kind = nordend::ui::PointerEvent::Kind;
    // A double click in the middle of the page: a whole word, Quick Look still open.
    const float middle_x = width / 2.0F;
    const float middle_y = (height - 44.0F) / 2.0F;
    pointer(Kind::press, middle_x, middle_y, 2);
    pointer(Kind::release, middle_x, middle_y, 2);
    CHECK(look.is_open());
    const std::string word = look.selected_text();
    CHECK_FALSE(word.empty());
    CHECK(word.find(' ') == std::string::npos);
    // A drag down three lines: more than a line, from that word on.
    pointer(Kind::press, middle_x, middle_y, 1);
    pointer(Kind::motion, middle_x, middle_y + (3.0F * QuickLook::kTextLineHeight), 0);
    pointer(Kind::release, middle_x, middle_y + (3.0F * QuickLook::kTextLineHeight), 0);
    CHECK(std::ranges::count(look.selected_text(), '\n') == 3);
    draw();  // the selection drawn behind the text
    // Beside the page: Quick Look closes.
    pointer(Kind::press, 5.0F, 5.0F, 1);
    CHECK_FALSE(look.is_open());
}
