#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// User settings from config.toml. Every key is optional: a missing key keeps its default, a bad
// value (wrong type, out of range) is reported and keeps its default, an unknown key is reported
// and ignored, and a file that does not parse is ignored as a whole. Reading a config never fails.
namespace nordend::core {

struct Config {
    struct Font {
        std::string family{"sans-serif"};    // fontconfig pattern, e.g. "Inter" or "Inter:size=11"
        std::string monospace{"monospace"};  // text previews, e.g. "JetBrains Mono"
    };

    struct Pointer {
        int double_click_ms{400};      // presses closer in time count as a double click
        int double_click_distance{4};  // ... and closer than this, in logical pixels
    };

    Font font;
    Pointer pointer;
};

struct ConfigResult {
    Config config;
    bool from_file{false};  // the file existed and parsed; false means pure defaults
    // One line per problem, ready to print: "<file>:<line>:<column>: <what>".
    std::vector<std::string> problems;
};

// $XDG_CONFIG_HOME/nordend/config.toml, else $HOME/.config/nordend/config.toml; empty when
// neither variable is set.
[[nodiscard]] std::filesystem::path default_config_path();

// Parses TOML text. source names the file in problem messages.
[[nodiscard]] ConfigResult parse_config(std::string_view text, std::string_view source);

// Reads and parses a file. A missing file is not a problem: defaults, from_file = false.
[[nodiscard]] ConfigResult load_config(const std::filesystem::path& path);

// Editing config.toml as text, so what a person wrote stays (ROADMAP 7.5c): the value of key in
// [section] is replaced on its line, keeping the indentation, the spacing and a comment after
// it; also when written as a dotted key at the top ("font.family = ..."). A missing key goes at
// the end of its section, a missing section at the end of the file. value is TOML as written
// in the file (see toml_string). Nothing else changes: comments, order, unknown keys.
[[nodiscard]] std::string set_config_value(std::string_view text, std::string_view section,
                                           std::string_view key, std::string_view value);

// What is wrong with key in [section] in a parsed config, as a person reads it ("must be between
// 100 and 2000, got 5"); a file that no longer parses is wrong too. Empty when nothing is.
[[nodiscard]] std::string config_problem(const ConfigResult& result, std::string_view section,
                                         std::string_view key);

// text as a TOML basic string: in double quotes, with \, \" and control characters escaped.
[[nodiscard]] std::string toml_string(std::string_view text);

}  // namespace nordend::core
