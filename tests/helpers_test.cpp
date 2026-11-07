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

#include "nordend/formats/image/load.hpp"
#include "nordend/formats/image/photo.hpp"
#include "nordend/picture/load.hpp"
#include "protocol.hpp"
#include "temp_dir.hpp"

namespace fs = std::filesystem;
namespace image = nordend::formats::image;
namespace picture = nordend::picture;
namespace isolate = nordend::isolate;
using nordend::test::TempDir;
using picture::Failure;

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

picture::LoadRequest request(const fs::path& path, std::uint32_t side = 128) {
    return {.path = path, .max_width = side, .max_height = side};
}

}  // namespace

TEST_CASE("helpers: the helper makes the same pictures as this process") {
    isolate::Helpers helpers{real()};
    for (const char* name : {"blocks.png", "blocks.jpg", "blocks.gif", "blocks.webp", "blocks.jxl",
                             "blocks-orient6.jpg", "shot.jpg", "with-thumbnail.jpg",
                             "tiff-preview.tif", "truncated.jpg"}) {
        INFO(name);
        const picture::Loaded there = helpers.load(request(kData / name));
        const picture::Loaded here = image::load(request(kData / name));
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

TEST_CASE("helpers: a photo's information from the helper, its sidecar's rating included") {
    isolate::Helpers helpers{real(1)};
    for (const char* name : {"shot.jpg", "with-thumbnail.jpg", "blocks.png", "truncated.jpg"}) {
        INFO(name);
        const picture::PhotoInfo there = helpers.photo_info(kData / name);
        const picture::PhotoInfo here = image::read_photo_info(kData / name);
        CHECK(there.width == here.width);
        CHECK(there.height == here.height);
        CHECK(there.camera == here.camera);
        CHECK(there.lens == here.lens);
        CHECK(there.exposure_time == here.exposure_time);
        CHECK(there.aperture == here.aperture);
        CHECK(there.iso == here.iso);
        CHECK(there.taken == here.taken);
        CHECK(there.software == here.software);
        CHECK(there.rating == here.rating);
    }
    const picture::PhotoInfo shot = helpers.photo_info(kData / "shot.jpg");
    CHECK(shot.camera == "OM-1");
    CHECK(shot.rating.has_value());  // from shot.jpg.xmp, sent as a second descriptor
    CHECK(helpers.photo_info(kData / "missing.jpg").camera.empty());
}

TEST_CASE("helpers: a photo's information travels whole, and nonsense is refused") {
    picture::PhotoInfo info;
    info.width = 5220;
    info.height = 3912;
    info.camera = "OM-1";
    info.lens = std::string(400, 'L');  // cut at 255 bytes
    info.rating = -1;
    const std::vector<std::byte> bytes = isolate::protocol::encode_info(info);
    const std::optional<picture::PhotoInfo> decoded = isolate::protocol::decode_info(bytes);
    REQUIRE(decoded.has_value());
    const picture::PhotoInfo back = decoded.value_or(picture::PhotoInfo{});
    CHECK(back.width == 5220);
    CHECK(back.camera == "OM-1");
    CHECK(back.lens == std::string(255, 'L'));
    CHECK(back.rating == -1);
    CHECK_FALSE(isolate::protocol::decode_info(std::span{bytes}.first(bytes.size() - 1)));
    std::vector<std::byte> longer = bytes;
    longer.push_back(std::byte{0});
    CHECK_FALSE(isolate::protocol::decode_info(longer));
    CHECK_FALSE(isolate::protocol::decode_info({}));
}

TEST_CASE("helpers: never more helpers than allowed, every request answered") {
    isolate::Helpers helpers{real(2)};
    std::vector<std::future<picture::Loaded>> results;
    results.reserve(12);
    for (int i = 0; i < 12; ++i) {
        results.push_back(std::async(std::launch::async, [&helpers] {
            return helpers.load(request(kData / "shot.jpg", 256));
        }));
    }
    for (std::future<picture::Loaded>& result : results) {
        const picture::Loaded got = result.get();
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
    const picture::Loaded left = leaving.load(request(kData / "blocks.png"));
    CHECK(left.failure == Failure::unavailable);
    CHECK(leaving.running() == 0);
}

TEST_CASE("helpers: a helper that crashes, hangs or talks nonsense is let go of") {
    for (const char* mode : {"crash", "hang", "garbage", "big-lie"}) {
        INFO(mode);
        isolate::Helpers helpers{fake(mode)};
        const auto start = std::chrono::steady_clock::now();
        const picture::Loaded loaded = helpers.load(request(kData / "blocks.png"));
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
    const picture::Loaded loaded = helpers.load(request(kData / "blocks.png"));
    CHECK(loaded.failure == Failure::unavailable);
    CHECK(loaded.detail == "the preview helper could not set up its sandbox");
    CHECK(helpers.running() == 0);
}

TEST_CASE("helpers: no helper, no picture, and nothing parsed here instead") {
    isolate::HelperOptions missing;
    missing.program = "/nonexistent/nordend-preview";
    isolate::Helpers helpers{missing};
    const picture::Loaded loaded = helpers.load(request(kData / "blocks.png"));
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
    picture::set_loader([helpers](const picture::LoadRequest& r) { return helpers->load(r); });
    CHECK(picture::load_picture(request(kData / "blocks.png")).failure == Failure::unavailable);
    // No loader: no picture, never made here instead.
    picture::set_loader({});
    CHECK(picture::load_picture(request(kData / "blocks.png")).failure == Failure::unavailable);
    // Back to what the other tests use: the image family in this process.
    picture::set_loader([](const picture::LoadRequest& r) { return image::load(r); });
    CHECK(picture::load_picture(request(kData / "blocks.png")).failure == Failure::none);
}
