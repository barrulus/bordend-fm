#include "rummage/theme/app_icon.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "rummage/core/event_loop.hpp"
#include "rummage/core/file.hpp"
#include "rummage/core/worker_pool.hpp"
#include "rummage/theme/desktop.hpp"
#include "rummage/theme/theme.hpp"
#include "rummage/ui/color.hpp"
#include "temp_dir.hpp"

using rummage::core::EventLoop;
using rummage::core::read_text_file;
using rummage::core::WorkerPool;
using rummage::test::TempDir;
using rummage::theme::app_icon_svg;
using rummage::theme::app_icon_tones;
using rummage::theme::AppIconChange;
using rummage::theme::AppIconKeeper;
using rummage::theme::AppIconPlace;
using rummage::theme::AppIconTones;
using rummage::theme::Color;
using rummage::theme::Desktop;
using rummage::theme::hex;
using rummage::theme::kIceTones;
using rummage::theme::sync_app_icon;
using rummage::theme::Theme;
using rummage::theme::written_by_rummage;
using namespace std::chrono_literals;

namespace {

float luminance(Color c) { return (0.2126F * c.r) + (0.7152F * c.g) + (0.0722F * c.b); }

std::string text_of(const std::filesystem::path& path) { return read_text_file(path).text; }

}  // namespace

TEST_CASE("app icon: without an accent it is the installed icon, byte for byte") {
    const std::string installed = text_of(RUMMAGE_APP_ICON);
    REQUIRE_FALSE(installed.empty());
    CHECK(app_icon_svg(std::nullopt) == installed);
    CHECK_FALSE(written_by_rummage(installed));
}

TEST_CASE("app icon: the installed front colour as accent gives back the installed tones") {
    const AppIconTones tones = app_icon_tones(kIceTones.front);
    CHECK(hex(tones.front) == "#6dbdf0");
    CHECK(hex(tones.tab) == "#3f8cc4");
    CHECK(hex(tones.mountain) == "#2a6a9e");
}

TEST_CASE("app icon: tones from four accents") {
    // Noctalia's default yellow, Nord's frost, GNOME's orange (chroma held back), a grey.
    struct Case {
        std::uint32_t accent{0};
        std::array<std::string_view, 3> tones;  // front, tab, mountain
    };

    constexpr std::array cases{
        Case{0xFFF59BU, {"#c0b65d", "#8d8a26", "#696b08"}},
        Case{0x88C0D0U, {"#86bece", "#568ea2", "#3c6d80"}},
        Case{0xE66100U, {"#ff925e", "#cf6100", "#9c4d00"}},
        Case{0x808080U, {"#b3b3b3", "#858585", "#656565"}},
    };
    for (const Case& c : cases) {
        CAPTURE(hex(Color::rgb(c.accent)));
        const AppIconTones tones = app_icon_tones(Color::rgb(c.accent));
        CHECK(hex(tones.front) == c.tones[0]);
        CHECK(hex(tones.tab) == c.tones[1]);
        CHECK(hex(tones.mountain) == c.tones[2]);
        // Front lighter than the tab, the tab lighter than the mountain, as in the installed icon.
        CHECK(luminance(tones.front) > luminance(tones.tab));
        CHECK(luminance(tones.tab) > luminance(tones.mountain));
        // The snow stays white whatever the accent.
        CHECK(app_icon_svg(Color::rgb(c.accent)).find("fill=\"#ffffff\"") != std::string::npos);
    }
}

TEST_CASE("app icon: a recoloured icon says rummage wrote it") {
    const std::string svg = app_icon_svg(Color::rgb(0x3584E4));
    CHECK(written_by_rummage(svg));
    CHECK(svg.find("#3584e4") != std::string::npos);
    CHECK(svg.starts_with("<?xml"));
}

TEST_CASE("app icon: sync writes only what changes and takes away only its own file") {
    const TempDir dir;
    const AppIconPlace place{.path = dir.path / "icons/hicolor/scalable/apps/rummage.svg"};
    const Color blue = Color::rgb(0x3584E4);
    const Color green = Color::rgb(0x3A944A);

    CHECK(sync_app_icon(place, std::nullopt) == AppIconChange::none);  // nothing there
    CHECK(sync_app_icon(place, blue) == AppIconChange::written);       // folders made
    CHECK(text_of(place.path) == app_icon_svg(blue));
    CHECK(sync_app_icon(place, blue) == AppIconChange::none);
    CHECK(sync_app_icon(place, green) == AppIconChange::written);
    CHECK(text_of(place.path) == app_icon_svg(green));
    CHECK(sync_app_icon(place, std::nullopt) == AppIconChange::removed);
    CHECK_FALSE(std::filesystem::exists(place.path));

    SUBCASE("someone else's icon is never touched") {
        dir.write("icons/hicolor/scalable/apps/rummage.svg", "<svg>mine</svg>");
        CHECK(sync_app_icon(place, blue) == AppIconChange::left_alone);
        CHECK(sync_app_icon(place, std::nullopt) == AppIconChange::none);
        CHECK(text_of(place.path) == "<svg>mine</svg>");
    }
    SUBCASE("no path, nothing to do") { CHECK(sync_app_icon({}, blue) == AppIconChange::none); }
}

TEST_CASE("app icon: installed under ~/.local, the installed icon comes back") {
    const TempDir dir;
    const AppIconPlace place{.path = dir.path / "rummage.svg", .installed_there = true};
    dir.write("rummage.svg", app_icon_svg(std::nullopt));  // cmake --install put it there

    CHECK(sync_app_icon(place, Color::rgb(0x9141AC)) == AppIconChange::written);
    CHECK(written_by_rummage(text_of(place.path)));
    CHECK(sync_app_icon(place, std::nullopt) == AppIconChange::restored);
    CHECK(text_of(place.path) == app_icon_svg(std::nullopt));
    CHECK(sync_app_icon(place, std::nullopt) == AppIconChange::none);
}

TEST_CASE("app icon: default place follows XDG_DATA_HOME") {
    // tests/main.cpp points XDG_DATA_HOME at the test root; the test binary is not installed.
    const AppIconPlace place = rummage::theme::default_app_icon_place();
    REQUIRE_FALSE(place.path.empty());
    CHECK(place.path.string().ends_with("/data/icons/hicolor/scalable/apps/rummage.svg"));
    CHECK_FALSE(place.installed_there);
}

TEST_CASE("app icon keeper: one job at a time, the latest accent wins") {
    const TempDir dir;
    EventLoop loop;
    loop.add_timer(10s, 0ms, [&loop] {
        FAIL("timed out");
        loop.quit(-1);
    });
    WorkerPool pool{loop};
    const AppIconPlace place{.path = dir.path / "rummage.svg"};
    AppIconKeeper keeper{pool, place};
    int jobs = 0;
    keeper.set_done_handler([&](AppIconChange) {
        ++jobs;
        if (text_of(place.path) == app_icon_svg(Color::rgb(0xE62D42))) {
            loop.quit(0);
        }
    });
    keeper.follow(Color::rgb(0x3584E4));
    keeper.follow(Color::rgb(0x3A944A));  // waits, then replaced by the next
    keeper.follow(Color::rgb(0xE62D42));
    CHECK(loop.run() == 0);
    CHECK(jobs == 2);
}

TEST_CASE("theme accent: colors.toml's primary, else the desktop's accent, else none") {
    const TempDir dir;
    const std::filesystem::path file = dir.path / "colors.toml";
    Theme theme{file};
    CHECK_FALSE(theme.accent());
    theme.set_desktop(Desktop{.dark = true, .accent = std::nullopt});
    CHECK_FALSE(theme.accent());
    theme.set_desktop(Desktop{.dark = true, .accent = Color::rgb(0x9141AC)});
    CHECK(theme.accent() == Color::rgb(0x9141AC));

    dir.write("colors.toml", "mode = \"dark\"\n[colors]\nprimary = \"#88c0d0\"\n");
    theme.reload();
    CHECK(theme.accent() == Color::rgb(0x88C0D0));
}
