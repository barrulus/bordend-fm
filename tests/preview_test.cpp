#include "nordend/preview/preview.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/mime.hpp"
#include "temp_dir.hpp"

namespace preview = nordend::preview;
namespace fs = std::filesystem;
using nordend::fs::MimeDatabase;
using nordend::test::TempDir;

namespace {

fs::path write(const fs::path& path, const std::string& bytes) {
    std::ofstream{path, std::ios::binary} << bytes;
    return path;
}

std::vector<std::string> lines_of(const fs::path& path, const preview::TextLimits& limits = {}) {
    std::string error;
    const std::optional<preview::Text> text = preview::read_text(path, limits, error);
    CHECK(text);
    CHECK(error.empty());
    return text.value_or(preview::Text{}).lines;
}

}  // namespace

TEST_CASE("preview: text is split into lines, tabs expanded") {
    const TempDir dir;
    CHECK(lines_of(write(dir.path / "a.txt", "one\ntwo\r\n\nthree")) ==
          std::vector<std::string>{"one", "two", "", "three"});
    CHECK(lines_of(write(dir.path / "b.txt", "end\n")) == std::vector<std::string>{"end"});
    CHECK(lines_of(write(dir.path / "c.txt", "")).empty());
    // Tab stops every 4 columns; a character of several bytes is one column.
    CHECK(lines_of(write(dir.path / "d.txt", "\tx\nab\tc\nè\tz")) ==
          std::vector<std::string>{"    x", "ab  c", "è   z"});
    // A UTF-8 byte order mark is not shown.
    CHECK(lines_of(write(dir.path / "e.txt", "\xEF\xBB\xBFhi")) == std::vector<std::string>{"hi"});
}

TEST_CASE("preview: binary files are not text") {
    const TempDir dir;
    std::string error;
    CHECK_FALSE(preview::read_text(write(dir.path / "a.bin", std::string{"a\0b", 3}), {}, error));
    CHECK(error.empty());
    // UTF-16 has NUL bytes too.
    CHECK_FALSE(
        preview::read_text(write(dir.path / "b.txt", std::string{"\xFF\xFEh\0i\0", 6}), {}, error));
    const std::optional<preview::Text> missing =
        preview::read_text(dir.path / "missing.txt", {}, error);
    CHECK_FALSE(error.empty());
    CHECK(missing.value_or(preview::Text{.lines = {"x"}, .truncated = false}).lines.empty());
}

TEST_CASE("preview: long files and long lines are cut") {
    const TempDir dir;
    // 10 bytes read of "aaa\nbbb\nccc\nddd": the incomplete third line is left out.
    std::string error;
    const std::optional<preview::Text> head = preview::read_text(
        write(dir.path / "a.txt", "aaa\nbbb\nccc\nddd"), {.max_bytes = 10}, error);
    const preview::Text head_text = head.value_or(preview::Text{});
    CHECK(head_text.truncated);
    CHECK(head_text.lines == std::vector<std::string>{"aaa", "bbb"});
    const std::optional<preview::Text> whole =
        preview::read_text(dir.path / "a.txt", {.max_bytes = 15}, error);
    const preview::Text whole_text = whole.value_or(preview::Text{.lines = {}, .truncated = true});
    CHECK_FALSE(whole_text.truncated);
    CHECK(whole_text.lines.size() == 4);

    // Lines are cut at a character boundary: "è" is two bytes.
    const preview::TextLimits narrow{.max_line_bytes = 4};
    CHECK(lines_of(write(dir.path / "b.txt", "abcdef\naèèè\n\tx"), narrow) ==
          std::vector<std::string>{"abcd", "aè", "    "});
}

TEST_CASE("preview: information on files, directories and links") {
    const TempDir dir;
    const nordend::core::CancelToken token;
    const fs::path file = write(dir.path / "a.txt", "a");
    fs::permissions(file, fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read);
    const preview::Info info = preview::read_info(file, token);
    CHECK(info.error.empty());
    CHECK(info.permissions == "-rw-r-----");
    CHECK_FALSE(info.owner.empty());
    CHECK_FALSE(info.group.empty());
    CHECK_FALSE(info.items);

    fs::create_directory(dir.path / "sub");
    write(dir.path / "sub" / "x", "");
    write(dir.path / "sub" / ".hidden", "");
    fs::create_directory(dir.path / "sub" / "deeper");
    fs::permissions(dir.path / "sub", fs::perms::owner_all | fs::perms::sticky_bit);
    const preview::Info folder = preview::read_info(dir.path / "sub", token);
    CHECK(folder.permissions == "drwx-----T");
    CHECK(folder.items == std::optional<std::uint64_t>{3});
    CHECK(folder.hidden_items == 1);

    fs::create_symlink("sub", dir.path / "link");
    const preview::Info link = preview::read_info(dir.path / "link", token);
    CHECK(link.link_target == "sub");
    CHECK(link.permissions.front() == 'd');  // the target's
    fs::create_symlink("nowhere", dir.path / "dangling");
    const preview::Info dangling = preview::read_info(dir.path / "dangling", token);
    CHECK(dangling.link_target == "nowhere");
    CHECK(dangling.permissions.front() == 'l');

    CHECK_FALSE(preview::read_info(dir.path / "missing", token).error.empty());
}

TEST_CASE("preview: the provider for a type") {
    const auto mime = MimeDatabase::load();
    const TempDir dir;
    const nordend::core::CancelToken token;
    const auto content_of = [&](const char* type, const fs::path& path) {
        return preview::provider_for(*mime, mime->find(type)).load({.path = path}, token);
    };
    const fs::path json = write(dir.path / "a.json", "{\n\t\"a\": 1\n}\n");
    const preview::Content text = content_of("application/json", json);
    REQUIRE(std::holds_alternative<preview::Text>(text));
    CHECK(std::get<preview::Text>(text).lines.size() == 3);
    // Text by name, binary inside: the information card.
    const fs::path fake = write(dir.path / "fake.txt", std::string{"\0\0", 2});
    CHECK(std::holds_alternative<preview::Info>(content_of("text/plain", fake)));
    CHECK(std::holds_alternative<preview::Info>(content_of("application/zip", json)));
    CHECK(std::holds_alternative<preview::Info>(content_of("inode/directory", dir.path)));
    CHECK(std::holds_alternative<nordend::image::Loaded>(
        content_of("image/png", fs::path{NORDEND_TEST_DATA} / "image" / "blocks.png")));
    // A type not known yet gets the card until it is.
    CHECK(std::holds_alternative<preview::Info>(
        preview::provider_for(*mime, nordend::fs::kUnknownMime).load({.path = json}, token)));
}

TEST_CASE("preview: the info pane's summary of a photo, a text file and a folder") {
    const TempDir dir;
    const nordend::core::CancelToken token;
    const preview::Summary photo = preview::summarize(
        fs::path{NORDEND_TEST_DATA} / "image" / "shot.jpg", preview::Kind::image, token);
    REQUIRE(photo.photo.has_value());
    CHECK(photo.photo.value_or(nordend::image::PhotoInfo{}).camera == "OM-1");
    CHECK_FALSE(photo.text);
    CHECK_FALSE(photo.info.permissions.empty());

    std::string many;
    for (int i = 0; i < 100; ++i) {
        many += "line\n";
    }
    const preview::Summary text =
        preview::summarize(write(dir.path / "a.txt", many), preview::Kind::text, token);
    const preview::Text lines = text.text.value_or(preview::Text{});
    CHECK(lines.lines.size() == preview::kSummaryLines);
    CHECK(lines.truncated);
    CHECK_FALSE(text.photo);

    const preview::Summary folder = preview::summarize(dir.path, preview::Kind::info, token);
    CHECK(folder.info.items == std::optional<std::uint64_t>{1});
}
