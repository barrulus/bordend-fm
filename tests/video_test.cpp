#include <chrono>
#include <filesystem>

#include <doctest/doctest.h>

#include "nordend/thumbs/cache.hpp"
#include "temp_dir.hpp"

namespace thumbs = nordend::thumbs;
namespace fs = std::filesystem;
using nordend::test::TempDir;

// A tiny YUV4MPEG2 video made here keeps the decoder test independent of encoders and codecs.
// Two solid-red frames, including a video shorter than the thumbnail frame sample.
namespace {
fs::path video(const TempDir& dir, const std::string& name) {
    const auto file = dir.path / name;
    std::ofstream out{file, std::ios::binary};
    out << "YUV4MPEG2 W160 H90 F25:1 Ip A1:1 C420jpeg\n";
    for (int frame = 0; frame < 2; ++frame) {
        out << "FRAME\n"
            << std::string(160 * 90, static_cast<char>(81))
            << std::string(80 * 45, static_cast<char>(90))
            << std::string(80 * 45, static_cast<char>(240));
    }
    return file;
}
}  // namespace

TEST_CASE("video thumbnails: short clips, quoted paths, cache and larger sizes") {
    TempDir dir;
    const auto file = video(dir, "a 'video'; $test.y4m");
    const thumbs::Cache cache{dir.path / "cache", "test"};
    const auto first = cache.get(file, thumbs::Size::normal, thumbs::Kind::video);
    INFO(first.error);
    REQUIRE(first.source == thumbs::Source::decoded);
    CHECK(first.image.width == 128);
    CHECK(first.image.height == 72);
    REQUIRE(first.image.rgba.size() >= 4);
    CHECK(first.image.rgba[0] > 220);
    CHECK(first.image.rgba[1] < 30);
    CHECK(first.image.rgba[2] < 30);
    const auto cached = cache.get(file, thumbs::Size::normal, thumbs::Kind::video);
    CHECK(cached.source == thumbs::Source::cache);
    CHECK(cached.image.rgba == first.image.rgba);
    const auto large = cache.get(file, thumbs::Size::x_large, thumbs::Kind::video);
    INFO(large.error);
    REQUIRE_FALSE(large.image.empty());
    CHECK(large.image.width <= 512);
    CHECK(large.image.height <= 512);
    fs::last_write_time(file, fs::last_write_time(file) + std::chrono::seconds{10});
    CHECK(cache.get(file, thumbs::Size::normal, thumbs::Kind::video).source ==
          thumbs::Source::decoded);
}

TEST_CASE("video thumbnails: cancellation does not poison the failure cache") {
    TempDir dir;
    const auto file = video(dir, "clip.y4m");
    const thumbs::Cache cache{dir.path / "cache", "test"};
    nordend::core::CancelToken token;
    token.cancel();
    CHECK(cache.get(file, thumbs::Size::normal, thumbs::Kind::video, token).source ==
          thumbs::Source::failed);
    CHECK_FALSE(fs::exists(dir.path / "cache/fail"));
    CHECK(cache.get(file, thumbs::Size::normal, thumbs::Kind::video).source ==
          thumbs::Source::decoded);
}

TEST_CASE("video thumbnails: damaged files fail and do not use image failure records") {
    TempDir dir;
    const auto file = video(dir, "clip.y4m");
    const thumbs::Cache cache{dir.path / "cache", "test"};
    CHECK(cache.get(file, thumbs::Size::normal).source == thumbs::Source::failed);
    CHECK(cache.get(file, thumbs::Size::normal, thumbs::Kind::video).source ==
          thumbs::Source::decoded);
    const auto broken = dir.write("bad.mp4", "not a video");
    CHECK(cache.get(broken, thumbs::Size::normal, thumbs::Kind::video).source ==
          thumbs::Source::failed);
    CHECK(cache.get(broken, thumbs::Size::normal, thumbs::Kind::video).error ==
          "failed before, and unchanged since");
}
