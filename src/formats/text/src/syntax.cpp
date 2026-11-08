#include "nordend/formats/text/syntax.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "nordend/text/text.hpp"

namespace nordend::formats::text {

using nordend::text::Span;
using nordend::text::Style;

// How a language's code is written, for the lexer driven by tables.
struct Syntax {
    std::span<const std::string_view> line_comments;  // to the end of the line: "//", "#"
    std::string_view block_open;                      // "/*"; empty: none
    std::string_view block_close;                     // "*/"
    std::string_view quotes;                          // characters that open a string
    bool cpp_raw_strings{false};   // R"delimiter( ... )delimiter", with u8, u, U, L before
    bool cpp_prefixes{false};      // u8"", u"", U"", L"": the prefix belongs to the string
    bool preprocessor{false};      // a line starting with # is a directive (C)
    bool digit_separators{false};  // 1'000'000 (C++14)
    std::span<const std::string_view> keywords;
    std::span<const std::string_view> types;
    std::span<const std::string_view> constants;
    bool camel_case_types{false};  // Registry, TextLayout: types
    bool suffix_t_types{false};    // size_t, uint32_t: types
    bool capital_macros{false};    // NORDEND_VERSION, EINTR: macros
};

struct Language {
    std::string_view name;
    std::span<const std::string_view> mime;
    std::span<const std::string_view> file_names;
    std::span<const std::string_view> interpreters;
    const Syntax* syntax;
};

namespace {

// ---------------------------------------------------------------------------
// The languages
// ---------------------------------------------------------------------------

constexpr std::array<std::string_view, 1> kSlashComments{"//"};

constexpr auto kCppKeywords = std::to_array<std::string_view>({"_Alignas",
                                                               "_Alignof",
                                                               "_Atomic",
                                                               "_Generic",
                                                               "_Noreturn",
                                                               "_Static_assert",
                                                               "_Thread_local",
                                                               "alignas",
                                                               "alignof",
                                                               "and",
                                                               "and_eq",
                                                               "asm",
                                                               "break",
                                                               "case",
                                                               "catch",
                                                               "class",
                                                               "co_await",
                                                               "co_return",
                                                               "co_yield",
                                                               "compl",
                                                               "concept",
                                                               "const",
                                                               "const_cast",
                                                               "consteval",
                                                               "constexpr",
                                                               "constinit",
                                                               "continue",
                                                               "decltype",
                                                               "default",
                                                               "delete",
                                                               "do",
                                                               "dynamic_cast",
                                                               "else",
                                                               "enum",
                                                               "explicit",
                                                               "export",
                                                               "extern",
                                                               "final",
                                                               "for",
                                                               "friend",
                                                               "goto",
                                                               "if",
                                                               "import",
                                                               "inline",
                                                               "module",
                                                               "mutable",
                                                               "namespace",
                                                               "new",
                                                               "noexcept",
                                                               "not",
                                                               "not_eq",
                                                               "operator",
                                                               "or",
                                                               "or_eq",
                                                               "override",
                                                               "private",
                                                               "protected",
                                                               "public",
                                                               "register",
                                                               "reinterpret_cast",
                                                               "requires",
                                                               "restrict",
                                                               "return",
                                                               "sizeof",
                                                               "static",
                                                               "static_assert",
                                                               "static_cast",
                                                               "struct",
                                                               "switch",
                                                               "template",
                                                               "this",
                                                               "thread_local",
                                                               "throw",
                                                               "try",
                                                               "typedef",
                                                               "typeid",
                                                               "typename",
                                                               "union",
                                                               "using",
                                                               "virtual",
                                                               "volatile",
                                                               "while",
                                                               "xor",
                                                               "xor_eq",
                                                               "_Bool",
                                                               "_Complex",
                                                               "_Imaginary",
                                                               "_Pragma",
                                                               "__asm__",
                                                               "__attribute__",
                                                               "__declspec",
                                                               "__inline",
                                                               "__restrict"});

constexpr auto kCppTypes = std::to_array<std::string_view>(
    {"array",    "auto",        "bool",  "byte",       "char",          "char16_t",      "char32_t",
     "char8_t",  "double",      "float", "function",   "int",           "long",          "map",
     "optional", "pair",        "set",   "shared_ptr", "short",         "signed",        "span",
     "string",   "string_view", "tuple", "unique_ptr", "unordered_map", "unordered_set", "unsigned",
     "variant",  "vector",      "void",  "wchar_t",    "weak_ptr",      "FILE",          "deque",
     "list",     "atomic"});

constexpr auto kCppConstants =
    std::to_array<std::string_view>({"NULL", "false", "nullptr", "true", "stderr", "stdout"});

constexpr Syntax kCpp{
    .line_comments = kSlashComments,
    .block_open = "/*",
    .block_close = "*/",
    .quotes = "\"'",
    .cpp_raw_strings = true,
    .cpp_prefixes = true,
    .preprocessor = true,
    .digit_separators = true,
    .keywords = kCppKeywords,
    .types = kCppTypes,
    .constants = kCppConstants,
    .camel_case_types = true,
    .suffix_t_types = true,
    .capital_macros = true,
};

constexpr std::array<std::string_view, 2> kCMime{"text/x-csrc", "text/x-chdr"};
constexpr std::array<std::string_view, 2> kCppMime{"text/x-c++src", "text/x-c++hdr"};

constexpr std::array<Language, 2> kLanguages{{
    {.name = "C", .mime = kCMime, .file_names = {}, .interpreters = {}, .syntax = &kCpp},
    {.name = "C++", .mime = kCppMime, .file_names = {}, .interpreters = {}, .syntax = &kCpp},
}};

// ---------------------------------------------------------------------------
// The lexer driven by tables
// ---------------------------------------------------------------------------

// Where a line ends inside something that goes on to the next.
struct State {
    enum class In : std::uint8_t { code, block_comment, raw_string };
    In in{In::code};
    std::string raw_close;  // a raw string's end: )delimiter"
};

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }

bool is_lower(char c) { return c >= 'a' && c <= 'z'; }

bool is_word_start(char c) {
    return is_upper(c) || is_lower(c) || c == '_' || static_cast<unsigned char>(c) >= 0x80U;
}

bool is_word(char c) { return is_word_start(c) || is_digit(c); }

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v'; }

// What the lexer looks up, made once for each syntax.
struct Tables {
    std::array<bool, 256> opens{};  // may start a comment, a string, a number or a word
    std::unordered_map<std::string_view, Style> words;  // keywords, constants and types
};

Tables make_tables(const Syntax& syntax) {
    Tables tables;
    const auto open = [&](char c) { tables.opens.at(static_cast<unsigned char>(c)) = true; };
    for (const std::string_view marker : syntax.line_comments) {
        open(marker.front());
    }
    if (!syntax.block_open.empty()) {
        open(syntax.block_open.front());
    }
    for (const char quote : syntax.quotes) {
        open(quote);
    }
    for (int c = 0; c < 256; ++c) {
        const auto byte = static_cast<char>(c);
        if (is_digit(byte) || is_word_start(byte) || byte == '.') {
            open(byte);
        }
    }
    // Later lists win: a keyword over a constant over a type.
    for (const auto& [list, style] :
         {std::pair{syntax.types, Style::type}, std::pair{syntax.constants, Style::number},
          std::pair{syntax.keywords, Style::keyword}}) {
        for (const std::string_view word : list) {
            tables.words[word] = style;
        }
    }
    return tables;
}

const Tables& tables_of(const Syntax& syntax);

class LineLexer {
public:
    LineLexer(const Syntax& syntax, const Tables& tables, std::string_view line, State& state,
              std::vector<Span>& out)
        : syntax_{syntax}, tables_{tables}, line_{line}, state_{state}, out_{out} {}

    void run() {
        if (state_.in == State::In::block_comment) {
            close_block_comment(0);
        } else if (state_.in == State::In::raw_string) {
            close_raw_string(0);
        }
        if (state_.in == State::In::code && pos_ == 0 && syntax_.preprocessor) {
            directive();
        }
        while (pos_ < line_.size() && state_.in == State::In::code) {
            step();
        }
    }

private:
    void emit(std::size_t start, std::size_t end, Style style) {
        if (end > start) {
            out_.push_back({.start = static_cast<std::uint32_t>(start),
                            .length = static_cast<std::uint32_t>(end - start),
                            .style = style});
        }
    }

    [[nodiscard]] bool at(std::string_view text) const {
        return !text.empty() && line_.substr(pos_).starts_with(text);
    }

    void close_block_comment(std::size_t start) {
        const std::size_t end = line_.find(syntax_.block_close, pos_);
        if (end == std::string_view::npos) {
            emit(start, line_.size(), Style::comment);
            pos_ = line_.size();
            state_.in = State::In::block_comment;
            return;
        }
        pos_ = end + syntax_.block_close.size();
        emit(start, pos_, Style::comment);
        state_.in = State::In::code;
    }

    void close_raw_string(std::size_t start) {
        const std::size_t end = line_.find(state_.raw_close, pos_);
        if (end == std::string_view::npos) {
            emit(start, line_.size(), Style::string);
            pos_ = line_.size();
            state_.in = State::In::raw_string;
            return;
        }
        pos_ = end + state_.raw_close.size();
        emit(start, pos_, Style::string);
        state_.in = State::In::code;
        state_.raw_close.clear();
    }

    // # include <file>: the directive as a macro, an included file as a string.
    void directive() {
        std::size_t i = 0;
        while (i < line_.size() && is_space(line_[i])) {
            ++i;
        }
        if (i >= line_.size() || line_[i] != '#') {
            return;
        }
        const std::size_t hash = i++;
        while (i < line_.size() && is_space(line_[i])) {
            ++i;
        }
        const std::size_t word = i;
        while (i < line_.size() && is_word(line_[i])) {
            ++i;
        }
        emit(hash, i, Style::function);
        const std::string_view name = line_.substr(word, i - word);
        pos_ = i;
        if (name == "include" || name == "include_next" || name == "import" || name == "embed") {
            while (pos_ < line_.size() && is_space(line_[pos_])) {
                ++pos_;
            }
            if (pos_ < line_.size() && line_[pos_] == '<') {
                const std::size_t close = line_.find('>', pos_);
                const std::size_t end = close == std::string_view::npos ? line_.size() : close + 1;
                emit(pos_, end, Style::string);
                pos_ = end;
            }
        }
    }

    void step() {
        const char c = line_[pos_];
        if (!tables_.opens.at(static_cast<unsigned char>(c))) {
            ++pos_;  // a space, an operator, punctuation
            return;
        }
        for (const std::string_view marker : syntax_.line_comments) {
            if (at(marker)) {
                emit(pos_, line_.size(), Style::comment);
                pos_ = line_.size();
                return;
            }
        }
        if (at(syntax_.block_open)) {
            const std::size_t start = pos_;
            pos_ += syntax_.block_open.size();
            close_block_comment(start);
            return;
        }
        if (syntax_.quotes.find(c) != std::string_view::npos) {
            string(pos_, pos_);
            return;
        }
        if (is_digit(c) || (c == '.' && pos_ + 1 < line_.size() && is_digit(line_[pos_ + 1]))) {
            number();
            return;
        }
        if (is_word_start(c)) {
            word();
            return;
        }
        ++pos_;
    }

    // A string opened by the quote at quote (its prefix from start), to its closing quote; to the
    // end of the line when it has none.
    void string(std::size_t start, std::size_t quote) {
        const char close = line_[quote];
        std::size_t i = quote + 1;
        while (i < line_.size() && line_[i] != close) {
            i += line_[i] == '\\' ? std::size_t{2} : std::size_t{1};
        }
        pos_ = std::min(i + 1, line_.size());
        emit(start, pos_, Style::string);
    }

    // R"delimiter( ... )delimiter": the prefix starts at start, the quote is at quote.
    bool raw_string(std::size_t start, std::size_t quote) {
        const std::size_t open = line_.find('(', quote + 1);
        if (open == std::string_view::npos || open - quote - 1 > 16) {
            return false;
        }
        const std::string_view delimiter = line_.substr(quote + 1, open - quote - 1);
        if (std::ranges::any_of(delimiter, [](char d) {
                return is_space(d) || d == '\\' || d == ')' || d == '"';
            })) {
            return false;
        }
        state_.raw_close = ")" + std::string{delimiter} + "\"";
        pos_ = open + 1;
        close_raw_string(start);
        return true;
    }

    void number() {
        const std::size_t start = pos_;
        const bool hex =
            line_.substr(start).starts_with("0x") || line_.substr(start).starts_with("0X");
        while (pos_ < line_.size()) {
            const char c = line_[pos_];
            const char before = pos_ > start ? line_[pos_ - 1] : '\0';
            const bool exponent_sign =
                (c == '+' || c == '-') &&
                (((before == 'e' || before == 'E') && !hex) || before == 'p' || before == 'P');
            const bool separator = c == '\'' && syntax_.digit_separators &&
                                   pos_ + 1 < line_.size() && is_word(line_[pos_ + 1]);
            if (!is_word(c) && c != '.' && !exponent_sign && !separator) {
                break;
            }
            ++pos_;
        }
        emit(start, pos_, Style::number);
    }

    void word() {
        const std::size_t start = pos_;
        while (pos_ < line_.size() && is_word(line_[pos_])) {
            ++pos_;
        }
        const std::string_view name = line_.substr(start, pos_ - start);
        if (pos_ < line_.size() && line_[pos_] == '"') {
            if (syntax_.cpp_raw_strings &&
                (name == "R" || name == "u8R" || name == "uR" || name == "UR" || name == "LR") &&
                raw_string(start, pos_)) {
                return;
            }
            if (syntax_.cpp_prefixes &&
                (name == "u8" || name == "u" || name == "U" || name == "L")) {
                string(start, pos_);
                return;
            }
        }
        if (syntax_.cpp_prefixes && pos_ < line_.size() && line_[pos_] == '\'' &&
            (name == "u8" || name == "u" || name == "U" || name == "L")) {
            string(start, pos_);
            return;
        }
        if (const std::optional<Style> style = classify(name)) {
            emit(start, pos_, *style);
        }
    }

    // What a word is; nullopt for a plain name.
    [[nodiscard]] std::optional<Style> classify(std::string_view name) const {
        if (const auto found = tables_.words.find(name); found != tables_.words.end()) {
            return found->second;
        }
        if (syntax_.suffix_t_types && name.size() > 2 && name.ends_with("_t")) {
            return Style::type;
        }
        if (syntax_.capital_macros && std::ranges::none_of(name, is_lower) &&
            std::ranges::count_if(name, is_upper) >= 2) {
            return Style::function;
        }
        // Called: the next thing after spaces is "(".
        std::size_t next = pos_;
        while (next < line_.size() && is_space(line_[next])) {
            ++next;
        }
        if (next < line_.size() && line_[next] == '(') {
            return Style::function;
        }
        if (syntax_.camel_case_types && is_upper(name.front()) &&
            std::ranges::any_of(name, is_lower)) {
            return Style::type;
        }
        return std::nullopt;
    }

    const Syntax& syntax_;
    const Tables& tables_;
    std::string_view line_;
    State& state_;
    std::vector<Span>& out_;
    std::size_t pos_{0};
};

// Every language's tables, made the first time any is needed.
const Tables& tables_of(const Syntax& syntax) {
    static const std::unordered_map<const Syntax*, Tables> kTables = [] {
        std::unordered_map<const Syntax*, Tables> all;
        for (const Language& language : kLanguages) {
            if (!all.contains(language.syntax)) {
                all.emplace(language.syntax, make_tables(*language.syntax));
            }
        }
        return all;
    }();
    return kTables.at(&syntax);
}

}  // namespace

const Language* language_for(std::string_view name, std::string_view mime,
                             std::string_view first_line) {
    for (const Language& language : kLanguages) {
        if (std::ranges::find(language.mime, mime) != language.mime.end()) {
            return &language;
        }
    }
    for (const Language& language : kLanguages) {
        if (std::ranges::find(language.file_names, name) != language.file_names.end()) {
            return &language;
        }
    }
    if (first_line.starts_with("#!")) {
        // #!/usr/bin/env python3 -u, #!/bin/sh: the program's own name, its version left on.
        std::string_view command = first_line.substr(2);
        const auto next_word = [&] {
            const std::size_t begin = command.find_first_not_of(" \t");
            if (begin == std::string_view::npos) {
                return std::string_view{};
            }
            const std::size_t end = command.find_first_of(" \t", begin);
            const std::string_view word = command.substr(begin, end - begin);
            command = end == std::string_view::npos ? std::string_view{} : command.substr(end);
            return word;
        };
        std::string_view program = next_word();
        program = program.substr(program.rfind('/') + 1);
        if (program == "env") {
            program = next_word();
            while (program.starts_with('-')) {
                program = next_word();
            }
        }
        for (const Language& language : kLanguages) {
            for (const std::string_view interpreter : language.interpreters) {
                if (program == interpreter ||
                    (program.starts_with(interpreter) &&
                     program.substr(interpreter.size()).find_first_not_of("0123456789.") ==
                         std::string_view::npos)) {
                    return &language;
                }
            }
        }
    }
    return nullptr;
}

std::string_view language_name(const Language& language) { return language.name; }

void highlight(nordend::text::Text& text, const Language& language) {
    text.language = language.name;
    text.spans.assign(text.lines.size(), {});
    const Tables& tables = tables_of(*language.syntax);
    State state;
    for (std::size_t i = 0; i < text.lines.size(); ++i) {
        LineLexer{*language.syntax, tables, text.lines[i], state, text.spans[i]}.run();
    }
}

void colour(nordend::text::Text& text, std::string_view name, std::string_view mime) {
    const Language* language =
        language_for(name, mime, text.lines.empty() ? std::string_view{} : text.lines.front());
    if (language != nullptr) {
        highlight(text, *language);
    }
}

}  // namespace nordend::formats::text
