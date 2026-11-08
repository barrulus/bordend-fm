#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// What Nordend does with a text file, wherever it is read (ROADMAP 3b): the Text it draws and
// how one is asked for. No reader here: the text family (formats/text) reads files in the base
// engine, and Nordend asks it through the loader, as for pictures.
namespace nordend::text {

// How the file's bytes were read. Every Text holds UTF-8 whatever the file's encoding.
enum class Encoding : std::uint8_t {
    utf8,     // valid UTF-8 (plain ASCII included), with or without a byte order mark
    utf16le,  // after its byte order mark
    utf16be,
    cp1252,  // not UTF-8: Windows-1252, the superset of Latin-1 browsers read "Latin-1" as
};

// Its name for people: "UTF-8", "UTF-16 LE", "UTF-16 BE", "Latin-1".
[[nodiscard]] std::string_view encoding_name(Encoding encoding) noexcept;

// The beginning of a text file, ready to draw line by line.
struct Text {
    std::vector<std::string> lines;  // UTF-8, tabs expanded, no line ends, never a '\n'
    bool truncated{false};           // the file goes on past what was read
    Encoding encoding{Encoding::utf8};
    bool crlf{false};  // some line ended with "\r\n"
};

struct Limits {
    std::size_t max_bytes{std::size_t{256} * 1024};  // read at most this much of the file
    std::size_t max_line_bytes{1024};                // longer lines are cut (minified files)
    std::size_t max_lines{0};                        // more are left out; 0: no limit
    std::size_t tab_width{4};
};

enum class Failure : std::uint8_t {
    none,
    binary,       // a NUL byte (or character): not text
    unreadable,   // the file could not be read
    unavailable,  // no engine could read it (none installed, or it failed)
};

struct LoadRequest {
    std::filesystem::path path;
    std::string mime;  // the type Nordend found, for the engine to choose a language by
    Limits limits;
};

struct Loaded {
    Failure failure{Failure::none};
    std::string detail;  // why, for the log, when it failed
    Text text;
};

// Where texts are read: Nordend sets its engine's helpers, tests and tools the text family in
// their own process. Set before use.
using Loader = std::function<Loaded(const LoadRequest&)>;
void set_loader(Loader loader);

// The text of request.path through the loader; unavailable when none is set. Thread-safe.
[[nodiscard]] Loaded load_text(const LoadRequest& request);

}  // namespace nordend::text
