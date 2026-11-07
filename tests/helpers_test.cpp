#include "nordend/isolate/helpers.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/image/load.hpp"
#include "temp_dir.hpp"

namespace fs = std::filesystem;
namespace image = nordend::image;
namespace isolate = nordend::isolate;
using image::Failure;
using nordend::test::TempDir;

namespace {

const fs::path kData = fs::path{NORDEND_TEST_DATA} / "image";

isolate::HelperOptions real(std::size_t max_helpers = 4) {
    isolate::HelperOptions options;
    options.program = NORDEND_PREVIEW_HELPER;
    options.max_helpers = max_helpers;
    return options;
}

isolate::HelperOptions fake(const char* mode) {
    isolate::HelperOptions options;
    options.program = NORDEND_FAKE_HELPER;
    options.args = {mode};
    options.max_helpers = 1;
    options.timeout = std::chrono::milliseconds{300};
    return options;
}

image::LoadRequest request(const fs::path& path, std::uint32_t side = 128) {
    return {.path = path, .max_width = side, .max_height = side};
}

}  // namespace

TEST_CASE("helpers: the helper makes the same pictures as this process") {
    isolate::Helpers helpers{real()};
    for (const char* name : {"blocks.png", "blocks.jpg", "blocks.gif", "blocks.webp", "blocks.jxl",
                             "blocks-orient6.jpg", "shot.jpg", "with-thumbnail.jpg",
                             "tiff-preview.tif", "truncated.jpg"}) {
        INFO(name);
        const image::Loaded there = helpers.load(request(kData / name));
        const image::Loaded here = image::load(request(kData / name));
        CHECK(there.failure == here.failure);
        CHECK(there.detail == here.detail);
        CHECK(there.from_preview == here.from_preview);
        CHECK(there.full_width == here.full_width);
        CHECK(there.full_height == here.full_height);
        CHECK(there.image.width == here.image.width);
        CHECK(there.image.rgba == here.image.rgba);
    }
    CHECK(helpers.running() == 1);  // one after the other: one helper did it all
}

TEST_CASE("helpers: never more helpers than allowed, every request answered") {
    isolate::Helpers helpers{real(2)};
    std::vector<std::future<image::Loaded>> results;
    results.reserve(12);
    for (int i = 0; i < 12; ++i) {
        results.push_back(std::async(std::launch::async, [&helpers] {
            return helpers.load(request(kData / "shot.jpg", 256));
        }));
    }
    for (std::future<image::Loaded>& result : results) {
        const image::Loaded got = result.get();
        INFO(got.detail);
        CHECK(got.failure == Failure::none);
    }
    CHECK(helpers.running() <= 2);
}

TEST_CASE("helpers: an idle helper is closed and waited for, then replaced unseen") {
    isolate::HelperOptions options = real(1);
    options.idle = std::chrono::milliseconds{50};
    isolate::Helpers helpers{options};
    CHECK(helpers.load(request(kData / "blocks.png")).failure == Failure::none);
    CHECK(helpers.running() == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds{300});
    CHECK(helpers.running() == 0);  // closed by Nordend and waited for: no zombie
    CHECK(helpers.load(request(kData / "blocks.png")).failure == Failure::none);
    // A goodbye racing with a request: sent again to a new one (which says goodbye too here).
    isolate::Helpers leaving{fake("goodbye")};
    const image::Loaded left = leaving.load(request(kData / "blocks.png"));
    CHECK(left.failure == Failure::unavailable);
    CHECK(leaving.running() == 0);
}

TEST_CASE("helpers: a helper that crashes, hangs or talks nonsense is let go of") {
    for (const char* mode : {"crash", "hang", "garbage", "big-lie"}) {
        INFO(mode);
        isolate::Helpers helpers{fake(mode)};
        const auto start = std::chrono::steady_clock::now();
        const image::Loaded loaded = helpers.load(request(kData / "blocks.png"));
        CHECK(loaded.failure == Failure::unavailable);
        CHECK(loaded.error() == "Preview unavailable");
        CHECK_FALSE(loaded.detail.empty());
        CHECK(loaded.image.empty());
        CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{2});
        CHECK(helpers.running() == 0);  // killed and waited for
    }
}

TEST_CASE("helpers: a helper without its cage never works, and Nordend says why") {
    isolate::Helpers helpers{fake("no-sandbox")};
    const image::Loaded loaded = helpers.load(request(kData / "blocks.png"));
    CHECK(loaded.failure == Failure::unavailable);
    CHECK(loaded.detail == "the preview helper could not set up its sandbox");
    CHECK(helpers.running() == 0);
}

TEST_CASE("helpers: no helper, no picture, and nothing parsed here instead") {
    isolate::HelperOptions missing;
    missing.program = "/nonexistent/nordend-preview";
    isolate::Helpers helpers{missing};
    const image::Loaded loaded = helpers.load(request(kData / "blocks.png"));
    CHECK(loaded.failure == Failure::unavailable);
    CHECK(loaded.image.empty());
    CHECK(helpers.running() == 0);
}

TEST_CASE("helpers: only regular files are sent") {
    const TempDir dir;
    isolate::Helpers helpers{real(1)};
    const fs::path fifo = dir.path / "pipe.jpg";
    REQUIRE(::mkfifo(fifo.c_str(), 0600) == 0);
    CHECK(helpers.load(request(fifo)).failure == Failure::unreadable);  // and returns at once
    CHECK(helpers.load(request(dir.path)).failure == Failure::unreadable);
    CHECK(helpers.load(request(dir.path / "missing.png")).failure == Failure::unreadable);
    CHECK(helpers.running() == 0);
}

TEST_CASE("helpers: load_picture goes where the loader says") {
    auto helpers = std::make_shared<isolate::Helpers>(fake("crash"));
    image::set_loader([helpers](const image::LoadRequest& r) { return helpers->load(r); });
    CHECK(image::load_picture(request(kData / "blocks.png")).failure == Failure::unavailable);
    image::set_loader({});
    CHECK(image::load_picture(request(kData / "blocks.png")).failure == Failure::none);
}
