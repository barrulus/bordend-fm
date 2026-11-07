#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/picture/load.hpp"
#include "nordend/picture/photo_info.hpp"

// What Quick Look shows for a file, made on a worker. One provider per kind of file (DESIGN,
// "Preview provider"): adding a kind does not touch the others, and the view draws what the
// provider made.
namespace nordend::preview {

// The beginning of a text file, ready to draw line by line.
struct Text {
    std::vector<std::string> lines;  // tabs expanded, no line ends; bytes as in the file
    bool truncated{false};           // the file goes on past what was read
};

// What the information card shows beyond the directory entry itself.
struct Info {
    std::string permissions;  // as ls shows them: "-rw-r--r--", "drwxr-xr-x"
    std::string owner;        // user and group names (numbers when unknown)
    std::string group;
    std::string link_target;                 // for a symbolic link, where it points; else empty
    std::optional<std::uint64_t> items;      // a directory's entries, hidden ones included
    std::uint64_t hidden_items{0};           // of items, the hidden ones (dotfiles)
    std::optional<std::int64_t> created_ns;  // birth time, where the file system keeps it
    std::string error;                       // the file could not be examined
};

using Content = std::variant<picture::Loaded, Text, Info>;

// What a provider makes: the alternative of Content it returns (Text may become Info: a "text"
// file with NUL bytes in it).
enum class Kind : std::uint8_t { image, text, info };

struct Request {
    std::filesystem::path path;
    // The area an image is shown in, in physical pixels.
    std::uint32_t width{0};
    std::uint32_t height{0};
};

class Provider {
public:
    Provider() = default;
    virtual ~Provider() = default;
    Provider(const Provider&) = delete;
    Provider& operator=(const Provider&) = delete;
    Provider(Provider&&) = delete;
    Provider& operator=(Provider&&) = delete;

    [[nodiscard]] virtual bool can_handle(const fs::MimeDatabase& mime, fs::MimeId type) const = 0;

    [[nodiscard]] virtual Kind kind() const = 0;

    // Blocking: call it on a worker. Never throws; problems end up in the content.
    [[nodiscard]] virtual Content load(const Request& request,
                                       const core::CancelToken& cancel) const = 0;
};

// The first provider that handles type: images and camera raw files, then text, then the
// information card, which handles everything.
[[nodiscard]] const Provider& provider_for(const fs::MimeDatabase& mime, fs::MimeId type);

// The building blocks of the providers.

struct TextLimits {
    std::size_t max_bytes{std::size_t{256} * 1024};  // read at most this much of the file
    std::size_t max_line_bytes{1024};                // longer lines are cut (minified files)
    std::size_t tab_width{4};
};

// The file's text; nullopt when it holds a NUL byte (binary, or UTF-16/32). A UTF-8 byte order
// mark is dropped; "\r\n" ends a line like "\n". Problems reading land in error.
[[nodiscard]] std::optional<Text> read_text(const std::filesystem::path& path,
                                            const TextLimits& limits, std::string& error);

// Permissions, owner, link target, birth time, and a directory's number of entries (stops
// counting when cancelled).
[[nodiscard]] Info read_info(const std::filesystem::path& path, const core::CancelToken& cancel);

// What the info pane shows about an entry, beyond the directory entry itself.
struct Summary {
    Info info;
    std::optional<picture::PhotoInfo> photo;  // images and camera raw files
    std::optional<Text> text;                 // the first lines of a text file
};

inline constexpr std::size_t kSummaryLines = 40;

// Reads it for an entry whose provider makes kind. Blocking: call it on a worker.
[[nodiscard]] Summary summarize(const std::filesystem::path& path, Kind kind,
                                const core::CancelToken& cancel);

}  // namespace nordend::preview
