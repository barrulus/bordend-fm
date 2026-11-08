#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "nordend/fs/mime.hpp"

// Nordend's engines (ROADMAP 3a): the programs that read files from outside, each in its cage
// (nordend-preview, the image engine, is the first). An engine is described by a manifest, a
// small TOML file installed with it in share/nordend/engines/: which types it shows, what it
// gives back, the protocol it speaks. Nordend reads the manifests at start-up and asks the
// registry who shows a type; it never parses a file itself. A new engine is a new manifest:
// nothing in views or picture changes.
//
//   [engine]
//   name = "image"
//   program = "nordend-preview"    # a file name, in libexec/nordend
//   protocol = 1
//
//   [[family]]
//   name = "image"
//   shows = "picture"
//   info = true                    # what the file says about itself (EXIF)
//   thumbnails = true
//   mime = ["image/png", "image/jpeg"]
//   subclasses = false             # true: their subtypes too (text/plain: every text type)
namespace nordend::engines {

// The protocol Nordend speaks with its engines (isolate/src/protocol.hpp): an engine whose
// manifest names another is skipped. Raised with every change to the messages.
inline constexpr std::int64_t kProtocol = 1;

// What a family gives back for Nordend to draw.
enum class Shows : std::uint8_t {
    picture,  // RGBA pixels (picture::Loaded)
};

struct Family {
    std::string name;
    Shows shows{Shows::picture};
    bool info{false};        // gives what the file says about itself (picture::PhotoInfo)
    bool thumbnails{false};  // its pictures go to the thumbnail cache too
    bool subclasses{false};  // the types listed and their subtypes; else exactly those
    std::vector<std::string> mime;
};

struct Manifest {
    std::string name;     // [a-z0-9-], for logs and --about
    std::string program;  // a file name, never a path
    std::vector<Family> families;
};

// The manifest in text, from source (for messages); nullopt with why when it is not one
// (malformed TOML, a key Nordend does not know, a value of the wrong type, another protocol).
[[nodiscard]] std::optional<Manifest> parse_manifest(std::string_view text, std::string_view source,
                                                     std::string& why);

struct Engine {
    Manifest manifest;
    std::filesystem::path program;        // where the program is
    std::filesystem::path manifest_path;  // where the manifest was read from (empty: made in code)
};

// Where an engine's program is, from its file name; empty when it is not there.
using Locate = std::function<std::filesystem::path(std::string_view program)>;

// The engines Nordend knows, and which family shows a type.
class Registry {
public:
    // Every *.toml in directory, in name order. An engine is skipped, with a warning in the
    // log, when its manifest is not one or its program cannot be found; a type already shown by
    // an earlier engine stays with that one. A missing directory gives no engines.
    [[nodiscard]] static Registry load(const std::filesystem::path& directory,
                                       const Locate& locate);

    // Adds an engine; false (and nothing added) when a type it lists is already shown by
    // another engine's family.
    bool add(Engine engine);

    // The family that shows type; nullptr when none does.
    [[nodiscard]] const Family* find(const fs::MimeDatabase& mime, fs::MimeId type) const;
    // The engine a family returned by find belongs to.
    [[nodiscard]] const Engine* engine_of(const Family& family) const;

    [[nodiscard]] std::span<const Engine> engines() const { return engines_; }

private:
    struct Place {
        std::size_t engine;
        std::size_t family;
    };

    [[nodiscard]] const Family& at(Place place) const;

    std::vector<Engine> engines_;
    std::unordered_map<std::string, Place> exact_;           // type name -> its family
    std::vector<std::pair<std::string, Place>> subclasses_;  // in the order added
};

// Where the manifests are: engines/ next to the running program (the build tree), else
// ../share/nordend/engines/ (installed). The first that exists; empty when neither does.
[[nodiscard]] std::filesystem::path default_directory();

// Where an engine's program is: next to the running program (the build tree), else in
// ../libexec/nordend/ (installed). Empty when neither exists, or when program is not a plain
// file name.
[[nodiscard]] std::filesystem::path locate_program(std::string_view program);

// The engines this process uses: set once at start-up, before anything asks; an empty registry
// until then. Thread-safe.
void set_installed(std::shared_ptr<const Registry> registry);
[[nodiscard]] std::shared_ptr<const Registry> installed();

}  // namespace nordend::engines
