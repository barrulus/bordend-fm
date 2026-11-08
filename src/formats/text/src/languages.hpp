#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "nordend/formats/text/syntax.hpp"
#include "nordend/text/text.hpp"

// The languages the text family colours, and how each one's code is written (private to the
// text family: the lexer in syntax.cpp reads these tables).
namespace nordend::formats::text {

// Strings whose end is a delimiter of the string's own choosing, without escapes.
enum class RawStrings : std::uint8_t {
    none,
    cpp,   // R"delimiter( ... )delimiter", with u8, u, U, L before the R
    rust,  // r"...", r#"..."#, with b before the r
};

// The tables in languages.cpp name only what they use: GCC's -Wmissing-field-initializers
// wants every member without an initializer given one, so each has one, which clang-tidy finds
// redundant.
// NOLINTBEGIN(readability-redundant-member-init)

// How a language's code is written, for the lexer driven by tables.
struct Syntax {
    std::span<const std::string_view> line_comments{};  // to the end of the line: "//", "#", "--"
    std::string_view block_open{};                      // "/*"; empty: none
    std::string_view block_close{};                     // "*/"
    bool nested_comments{false};                        // /* /* */ */ is one comment (Rust)
    bool lua_long_brackets{false};        // --[==[ comments ]==] and [[ strings ]], any number of =
    std::string_view quotes{};            // characters that open a string
    std::string_view multiline_quotes{};  // of those, the ones whose strings go on over lines
    bool triple_quotes{false};            // """ and ''' strings, over lines (Python)
    // Words glued before a quote that belong to the string: u8"", b"", f"" (compared in lower
    // case when prefix_any_case); with an r among their letters (raw_letter), no escapes.
    std::span<const std::string_view> string_prefixes{};
    bool prefix_any_case{false};
    char raw_letter{'\0'};
    RawStrings raw_strings{RawStrings::none};
    bool preprocessor{false};      // a line starting with # is a directive (C)
    bool digit_separators{false};  // 1'000'000 (C++14)
    char decorator{'\0'};          // @name: a decorator (Python, TypeScript)
    bool rust_attributes{false};   // #[attribute] and #![attribute]
    bool bang_macros{false};       // name! is a macro (Rust)
    bool lifetimes{false};         // 'a is a lifetime, 'a' a character (Rust)
    std::span<const std::string_view> keywords{};
    std::span<const std::string_view> types{};
    std::span<const std::string_view> constants{};
    bool camel_case_types{false};                  // Registry, TextLayout: types
    bool suffix_t_types{false};                    // size_t, uint32_t: types
    std::optional<nordend::text::Style> capitals;  // NORDEND_VERSION: a macro (C), a constant
};

struct Language {
    std::string_view name{};
    std::span<const std::string_view> mime{};
    std::span<const std::string_view> extensions{};    // ".rs": when the type says nothing better
    std::span<const std::string_view> file_names{};    // whole names: "Makefile"
    std::span<const std::string_view> interpreters{};  // after #!: "python3", "env node"
    const Syntax* syntax{};
};

// NOLINTEND(readability-redundant-member-init)

[[nodiscard]] std::span<const Language> all_languages();

}  // namespace nordend::formats::text
